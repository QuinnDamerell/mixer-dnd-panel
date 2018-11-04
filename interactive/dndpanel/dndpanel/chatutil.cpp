#include "chatutil.h"

#include "config.h"
#include "chatbot.h"

#include <windows.h>
#include <shellapi.h>

#include "../Chat/ChatHandler.h"

#undef GetObject

using namespace Chat;
using namespace ChatBot;
using namespace ChatSession;
using namespace rapidjson;


Chat::chat_event_internal::chat_event_internal(chat_event_type type) : type(type) {}

int Chat::create_method_json(ChatHandlerPtr chatHandler, const std::string& method, on_get_params getParams, bool discard, unsigned int* id, std::shared_ptr<rapidjson::Document>& methodDoc)
{
	std::shared_ptr<rapidjson::Document> doc(std::make_shared<rapidjson::Document>());
	doc->SetObject();
	rapidjson::Document::AllocatorType& allocator = doc->GetAllocator();

	unsigned int packetID = chatHandler->packetId++;
	doc->AddMember(RPC_ID, packetID, allocator);
	doc->AddMember(RPC_METHOD, method, allocator);
	doc->AddMember(RPC_DISCARD, discard, allocator);
	doc->AddMember(RPC_SEQUENCE, chatHandler->sequenceId, allocator);

	// Get the parameters from the caller.
	rapidjson::Value params(rapidjson::kObjectType);
	if (getParams)
	{
		getParams(allocator, params);
	}
	doc->AddMember(RPC_PARAMS, params, allocator);

	if (nullptr != id)
	{
		*id = packetID;
	}
	methodDoc = doc;
	return MIXER_OK;
}


// Queue a method to be sent out on the websocket. If handleImmediately is set to true, the handler will be called by the websocket receive thread rather than put on the reply queue.
int Chat::queue_method(ChatHandlerPtr chatHandler, const std::string& method, on_get_params getParams, method_handler_c onReply, const bool handleImmediately)
{
	std::shared_ptr<rapidjson::Document> methodDoc;
	unsigned int packetId = 0;
	RETURN_IF_FAILED(create_method_json(chatHandler, method, getParams, nullptr == onReply, &packetId, methodDoc));
	//DnDPanel::Logger::Info(std::string("Queueing method: ") + mixer_internal::jsonStringify(*methodDoc));
	if (onReply)
	{
		std::unique_lock<std::mutex> incomingLock(chatHandler->incomingMutex);
		chatHandler->replyHandlersById[packetId] = std::pair<bool, method_handler_c>(handleImmediately, onReply);
	}

	// Synchronize write access to the queue.
	std::shared_ptr<rpc_method_event> methodEvent = std::make_shared<rpc_method_event>(std::move(methodDoc));
	std::unique_lock<std::mutex> queueLock(chatHandler->outgoingMutex);
	chatHandler->outgoingEvents.emplace(methodEvent);
	chatHandler->outgoingCV.notify_one();

	return MIXER_OK;
}

int Chat::send_ready_message(ChatHandlerPtr chatHandler, bool ready)
{
	return queue_method(chatHandler, RPC_METHOD_READY, [&](rapidjson::Document::AllocatorType& allocator, rapidjson::Value& params)
	{
		params.AddMember(RPC_PARAM_IS_READY, ready, allocator);
	}, nullptr, false);
}

int Chat::handle_hello(BotPtr bot, rapidjson::Document& doc)
{
	(doc);
	if (bot->chatHandler->shutdownRequested)
	{
		return MIXER_OK;
	}

	//return bootstrap(session);

	return MIXER_OK;
}


void Chat::parse_participant(rapidjson::Value& participantJson, chat_participant& participant)
{
	participant.id = participantJson[RPC_SESSION_ID].GetString();
	participant.idLength = participantJson[RPC_SESSION_ID].GetStringLength();
	participant.userId = participantJson[RPC_USER_ID].GetUint();
	participant.userName = participantJson[RPC_USERNAME].GetString();
	participant.usernameLength = participantJson[RPC_USERNAME].GetStringLength();
	participant.level = participantJson[RPC_LEVEL].GetUint();
	participant.lastInputAtMs = participantJson[RPC_PART_LAST_INPUT].GetUint64();
	participant.connectedAtMs = participantJson[RPC_PART_CONNECTED].GetUint64();
	participant.disabled = participantJson[RPC_DISABLED].GetBool();
	participant.groupId = participantJson[RPC_GROUP_ID].GetString();
	participant.groupIdLength = participantJson[RPC_GROUP_ID].GetStringLength();
}

int Chat::cache_new_control(chat_session_internal& session, const char* sceneId, chat_control& control, rapidjson::Value& controlJson)
{
	// Critical Section: Add a control to the scenes cache.
	{
		std::unique_lock<std::shared_mutex> scenesLock(session.scenesMutex);
		if (session.controls.find(control.id) != session.controls.end())
		{
			return MIXER_ERROR_OBJECT_EXISTS;
		}

		auto sceneItr = session.scenes.find(sceneId);
		if (sceneItr == session.scenes.end())
		{
			return MIXER_ERROR_OBJECT_NOT_FOUND;
		}

		std::string scenePtr = sceneItr->second;
		rapidjson::Value* scene = rapidjson::Pointer(rapidjson::StringRef(scenePtr.c_str(), scenePtr.length())).Get(session.scenesRoot);

		rapidjson::Document::AllocatorType& allocator = session.scenesRoot.GetAllocator();
		auto controlsItr = scene->FindMember(RPC_PARAM_CONTROLS);
		rapidjson::Value* controls;
		if (controlsItr == scene->MemberEnd() || !controlsItr->value.IsArray())
		{
			controls = &scene->AddMember(RPC_PARAM_CONTROLS, rapidjson::Value(rapidjson::kArrayType), allocator);
		}
		else
		{
			controls = &controlsItr->value;
		}

		rapidjson::Value myControlJson(rapidjson::kObjectType);
		myControlJson.CopyFrom(controlJson, session.scenesRoot.GetAllocator());
		controls->PushBack(myControlJson, allocator);
	}
	
	return MIXER_OK;
}

int Chat::delete_cached_control(chat_session_internal& session, const char* sceneId, chat_control& control)
{
	// Critical Section: Erase the control if it exists on the scene.
	{
		std::unique_lock<std::shared_mutex> scenesLock(session.scenesMutex);
		if (session.controls.find(control.id) == session.controls.end())
		{
			// This control doesn't exist, ignore this deletion.
			return MIXER_OK;
		}

		auto sceneItr = session.scenes.find(sceneId);
		if (sceneItr == session.scenes.end())
		{
			return MIXER_ERROR_OBJECT_NOT_FOUND;
		}

		// Find the controls array on the scene.
		std::string scenePtr = sceneItr->second;
		rapidjson::Value* scene = rapidjson::Pointer(rapidjson::StringRef(scenePtr.c_str(), scenePtr.length())).Get(session.scenesRoot);
		auto controlsItr = scene->FindMember(RPC_PARAM_CONTROLS);
		if (controlsItr == scene->MemberEnd() || !controlsItr->value.IsArray())
		{
			// If the scene has no controls on it, ignore this deletion.
			return MIXER_OK;
		}

		// Erase the value from the array.
		rapidjson::Value* controls = &controlsItr->value;
		for (rapidjson::Value* controlItr = controls->Begin(); controlItr != controls->End(); ++controlItr)
		{
			if (0 == strcmp(controlItr->GetObject()[RPC_CONTROL_ID].GetString(), control.id))
			{
				controls->Erase(controlItr);
				break;
			}
		}
	}
	
	return MIXER_OK;
}

void Chat::parse_control(rapidjson::Value& controlJson, chat_control& control)
{
	control.id = controlJson[RPC_CONTROL_ID].GetString();
	control.idLength = controlJson[RPC_CONTROL_ID].GetStringLength();
	if (controlJson.HasMember(RPC_CONTROL_KIND))
	{
		control.kind = controlJson[RPC_CONTROL_KIND].GetString();
		control.kindLength = controlJson[RPC_CONTROL_KIND].GetStringLength();
	}
}

void Chat::chat_close_session(BotPtr chatBot)
{

	// Mark the session as inactive and close the websocket. This will notify any functions in flight to exit at their earliest convenience.
	chatBot->chatHandler->shutdownRequested = true;
	if (nullptr != chatBot->chatHandler->ws.get())
	{
		chatBot->chatHandler->ws->close();
	}

	// Notify the outgoing websocket thread to shutdown.
	{
		std::unique_lock<std::mutex> outgoingLock(chatBot->chatHandler->outgoingMutex);
		chatBot->chatHandler->outgoingCV.notify_all();
	}

	// Wait for both threads to terminate.
	if (chatBot->chatHandler->incomingThread.joinable())
	{
		chatBot->chatHandler->incomingThread.join();
	}
	if (chatBot->chatHandler->outgoingThread.joinable())
	{
		chatBot->chatHandler->outgoingThread.join();
	}
}