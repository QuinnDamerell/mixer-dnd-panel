#include "chatutil.h"
#include "config.h"
#include "chatbot.h"

#include <windows.h>
#include <shellapi.h>

#undef GetObject

using namespace Chat;
using namespace ChatBot;
using namespace ChatSession;
using namespace rapidjson;


Chat::chat_event_internal::chat_event_internal(chat_event_type type) : type(type) {}

int Chat::update_control_pointers(chat_session_internal& session, const char* sceneId)
{
	std::unique_lock<std::shared_mutex> scenesLock(session.scenesMutex);
	// Iterate through each scene and set up a pointer to each control.
	int sceneIndex = 0;
	for (auto& scene : session.scenesRoot[RPC_PARAM_SCENES].GetArray())
	{
		std::string scenePointer = "/" + std::string(RPC_PARAM_SCENES) + "/" + std::to_string(sceneIndex++);
		auto thisSceneId = scene[RPC_SCENE_ID].GetString();
		if (nullptr != sceneId)
		{
			if (0 != strcmp(sceneId, thisSceneId))
			{
				continue;
			}
		}

		auto controlsArray = scene.FindMember(RPC_PARAM_CONTROLS);
		if (controlsArray != scene.MemberEnd() && controlsArray->value.IsArray())
		{
			int controlIndex = 0;
			for (auto& control : controlsArray->value.GetArray())
			{
				chat_control_pointer ctrl(thisSceneId, scenePointer + "/" + std::string(RPC_PARAM_CONTROLS) + "/" + std::to_string(controlIndex++));
				session.controls.emplace(control[RPC_CONTROL_ID].GetString(), std::move(ctrl));
			}
		}

		session.scenes.emplace(thisSceneId, scenePointer);
	}

	return MIXER_OK;
}

int Chat::update_cached_control(chat_session_internal& session, chat_control& control, rapidjson::Value& controlJson)
{
	// Critical Section: Replace a control in the scenes cache.
	{
		std::unique_lock<std::shared_mutex> scenesLock(session.scenesMutex);
		auto itr = session.controls.find(control.id);
		if (itr == session.controls.end())
		{
			return MIXER_ERROR_OBJECT_NOT_FOUND;
		}

		std::string controlPtr = itr->second.cachePointer;
		rapidjson::Value myControlJson(rapidjson::kObjectType);
		myControlJson.CopyFrom(controlJson, session.scenesRoot.GetAllocator());
		rapidjson::Pointer(rapidjson::StringRef(controlPtr.c_str(), controlPtr.length()))
			.Swap(session.scenesRoot, myControlJson);
	}

	return MIXER_OK;
}

int Chat::create_method_json(chat_session_internal& session, const std::string& method, on_get_params getParams, bool discard, unsigned int* id, std::shared_ptr<rapidjson::Document>& methodDoc)
{
	std::shared_ptr<rapidjson::Document> doc(std::make_shared<rapidjson::Document>());
	doc->SetObject();
	rapidjson::Document::AllocatorType& allocator = doc->GetAllocator();

	unsigned int packetID = session.packetId++;
	doc->AddMember(RPC_ID, packetID, allocator);
	doc->AddMember(RPC_METHOD, method, allocator);
	doc->AddMember(RPC_DISCARD, discard, allocator);
	doc->AddMember(RPC_SEQUENCE, session.sequenceId, allocator);

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
int Chat::queue_method(chat_session_internal& session, const std::string& method, on_get_params getParams, method_handler_c onReply, const bool handleImmediately)
{
	std::shared_ptr<rapidjson::Document> methodDoc;
	unsigned int packetId = 0;
	RETURN_IF_FAILED(create_method_json(session, method, getParams, nullptr == onReply, &packetId, methodDoc));
	//DnDPanel::Logger::Info(std::string("Queueing method: ") + mixer_internal::jsonStringify(*methodDoc));
	if (onReply)
	{
		std::unique_lock<std::mutex> incomingLock(session.incomingMutex);
		session.replyHandlersById[packetId] = std::pair<bool, method_handler_c>(handleImmediately, onReply);
	}

	// Synchronize write access to the queue.
	std::shared_ptr<rpc_method_event> methodEvent = std::make_shared<rpc_method_event>(std::move(methodDoc));
	std::unique_lock<std::mutex> queueLock(session.outgoingMutex);
	session.outgoingEvents.emplace(methodEvent);
	session.outgoingCV.notify_one();

	return MIXER_OK;
}

int Chat::send_ready_message(chat_session_internal& session, bool ready)
{
	return queue_method(session, RPC_METHOD_READY, [&](rapidjson::Document::AllocatorType& allocator, rapidjson::Value& params)
	{
		params.AddMember(RPC_PARAM_IS_READY, ready, allocator);
	}, nullptr, false);
}

int Chat::cache_scenes(chat_session_internal& session)
{
	DnDPanel::Logger::Info("Caching scenes.");

	RETURN_IF_FAILED(queue_method(session, RPC_METHOD_GET_SCENES, nullptr, [](chat_session_internal& session, rapidjson::Document& doc) -> int
	{
		if (session.shutdownRequested)
		{
			return MIXER_OK;
		}

		// Critical Section: Get the scenes array from the result and set up pointers to scenes and controls.
		{
			std::unique_lock<std::shared_mutex> l(session.scenesMutex);
			session.controls.clear();
			session.scenes.clear();
			session.scenesRoot.RemoveAllMembers();

			// Copy just the scenes array portion of the reply into the cached scenes root.
			rapidjson::Value scenesArray(rapidjson::kArrayType);
			rapidjson::Value replyScenesArray = doc[RPC_RESULT][RPC_PARAM_SCENES].GetArray();
			scenesArray.CopyFrom(replyScenesArray, session.scenesRoot.GetAllocator());
			session.scenesRoot.AddMember(RPC_PARAM_SCENES, scenesArray, session.scenesRoot.GetAllocator());
		}

		RETURN_IF_FAILED(update_control_pointers(session, nullptr));
		if (!session.scenesCached)
		{
			session.scenesCached = true;
			return bootstrap(session);
		}

		return MIXER_OK;
	}));

	return MIXER_OK;
}

int Chat::cache_groups(chat_session_internal& session)
{
	DnDPanel::Logger::Info("Caching groups.");
	RETURN_IF_FAILED(queue_method(session, RPC_METHOD_GET_GROUPS, nullptr, [](chat_session_internal& session, rapidjson::Document& reply) -> int
	{
		scenes_by_group scenesByGroup;
		rapidjson::Value& groups = reply[RPC_RESULT][RPC_PARAM_GROUPS];
		for (auto& group : groups.GetArray())
		{
			std::string groupId = group[RPC_GROUP_ID].GetString();
			std::string sceneId;
			if (group.HasMember(RPC_SCENE_ID))
			{
				sceneId = group[RPC_SCENE_ID].GetString();
			}

			scenesByGroup.emplace(groupId, sceneId);
		}

		// Critical Section: Cache groups and their scenes
		{
			std::unique_lock<std::shared_mutex> l(session.scenesMutex);
			session.scenesByGroup.swap(scenesByGroup);
		}

		if (!session.groupsCached)
		{
			session.groupsCached = true;
			return bootstrap(session);
		}

		return MIXER_OK;
	}));

	return MIXER_OK;
}

/*
	Bootstrapping Interactive

	A few things must happen before the interactive connection is ready for the client to use.

	- First, the server time offset needs to be calculated in order to properly synchronize the client's clock for setting control cooldowns.
	- Second, in order to give the caller information about the interactive world, that data must be fetched from the server and cached.

	Once this information is requested, the bootstraping state is checked on each reply. If all items are complete the caller is informed via a state change event and the client is ready.
	*/
int Chat::bootstrap(chat_session_internal& session)
{
	DnDPanel::Logger::Info("Checking bootstrap state.");
	assert(interactive_connecting == session.state);

	if (!session.serverTimeOffsetCalculated)
	{
		RETURN_IF_FAILED(update_server_time_offset(session));
	}
	else if (!session.scenesCached)
	{
		RETURN_IF_FAILED(cache_scenes(session));
	}
	else if (!session.groupsCached)
	{
		RETURN_IF_FAILED(cache_groups(session));
	}
	else
	{
		DnDPanel::Logger::Info("Bootstrapping complete.");
		chat_state prevState = session.state;
		session.state = chat_connected;

		if (session.onStateChanged)
		{
			session.onStateChanged(session.callerContext, &session, prevState, session.state);
		}

		if (session.isReady)
		{
			return send_ready_message(session);
		}
	}

	return MIXER_OK;
}

int Chat::update_server_time_offset(chat_session_internal& session)
{
	// Calculate the server time offset.
	DnDPanel::Logger::Info("Requesting server time to calculate client offset.");

	int err = queue_method(session, RPC_METHOD_GET_TIME, nullptr, [](chat_session_internal& session, rapidjson::Document& doc) -> int
	{
		// Note: This reply handler is executed immediately by the background websocket thread.
		// Take care not to call callbacks that the user may expect on their own thread, the debug callback being the only exception.
		if (!doc.HasMember(RPC_RESULT) || !doc[RPC_RESULT].HasMember(RPC_TIME))
		{
			DnDPanel::Logger::Error("Unexpected reply format for server time reply");
			return MIXER_ERROR_UNRECOGNIZED_DATA_FORMAT;
		}

		auto receivedTime = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now());
		auto latency = (receivedTime - session.getTimeSent) / 2;
		unsigned long long serverTime = doc[RPC_RESULT][RPC_TIME].GetUint64();
		auto offset = receivedTime.time_since_epoch() - latency - std::chrono::milliseconds(serverTime);
		session.serverTimeOffsetMs = offset.count();
		DnDPanel::Logger::Info("Server time offset: " + std::to_string(session.serverTimeOffsetMs));

		// Continue bootstrapping the session.
		session.serverTimeOffsetCalculated = true;
		return bootstrap(session);
	}, true);
	if (err)
	{
		DnDPanel::Logger::Error("Method "  RPC_METHOD_GET_TIME " failed: " + std::to_string(err));
		return err;
	}

	session.getTimeSent = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now());

	return MIXER_OK;
}

int Chat::handle_hello(chat_session_internal& session, rapidjson::Document& doc)
{
	(doc);
	if (session.shutdownRequested)
	{
		return MIXER_OK;
	}

	return bootstrap(session);
}

int Chat::handle_input(chat_session_internal& session, rapidjson::Document& doc)
{
	if (!session.onInput)
	{
		// No input handler, return.
		return MIXER_OK;
	}

	interactive_input inputData;
	memset(&inputData, 0, sizeof(inputData));
	std::string inputJson = mixer_internal::jsonStringify(doc[RPC_PARAMS]);
	inputData.jsonData = inputJson.c_str();
	inputData.jsonDataLength = inputJson.length();
	rapidjson::Value& input = doc[RPC_PARAMS][RPC_PARAM_INPUT];
	inputData.control.id = input[RPC_CONTROL_ID].GetString();
	inputData.control.idLength = input[RPC_CONTROL_ID].GetStringLength();

	if (doc[RPC_PARAMS].HasMember(RPC_PARTICIPANT_ID))
	{
		inputData.participantId = doc[RPC_PARAMS][RPC_PARTICIPANT_ID].GetString();
		inputData.participantIdLength = doc[RPC_PARAMS][RPC_PARTICIPANT_ID].GetStringLength();
	}

	// Locate the cached control data.
	auto itr = session.controls.find(inputData.control.id);
	if (itr == session.controls.end())
	{
		int errCode = MIXER_ERROR_OBJECT_NOT_FOUND;
		if (session.onError)
		{
			std::string errMessage = "Input received for unknown control.";
			session.onError(session.callerContext, &session, errCode, errMessage.c_str(), errMessage.length());
		}

		return errCode;
	}

	rapidjson::Value* control = rapidjson::Pointer(itr->second.cachePointer.c_str()).Get(session.scenesRoot);
	if (nullptr == control)
	{
		int errCode = MIXER_ERROR_OBJECT_NOT_FOUND;
		if (session.onError)
		{
			std::string errMessage = "Internal failure: Failed to find control in cached json data.";
			session.onError(session.callerContext, &session, errCode, errMessage.c_str(), errMessage.length());
		}

		return errCode;
	}

	inputData.control.kind = control->GetObject()[RPC_CONTROL_KIND].GetString();
	inputData.control.kindLength = control->GetObject()[RPC_CONTROL_KIND].GetStringLength();
	if (doc[RPC_PARAMS].HasMember(RPC_PARAM_TRANSACTION_ID))
	{
		inputData.transactionId = doc[RPC_PARAMS][RPC_PARAM_TRANSACTION_ID].GetString();
		inputData.transactionIdLength = doc[RPC_PARAMS][RPC_PARAM_TRANSACTION_ID].GetStringLength();
	}

	std::string inputEvent = input[RPC_PARAM_INPUT_EVENT].GetString();
	if (0 == inputEvent.compare(RPC_INPUT_EVENT_MOVE))
	{
		inputData.type = input_type_move;
		inputData.coordinateData.x = input[RPC_INPUT_EVENT_MOVE_X].GetFloat();
		inputData.coordinateData.y = input[RPC_INPUT_EVENT_MOVE_Y].GetFloat();
	}
	else if (0 == inputEvent.compare(RPC_INPUT_EVENT_KEY_DOWN) ||
		0 == inputEvent.compare(RPC_INPUT_EVENT_KEY_UP))
	{
		inputData.type = input_type_key;
		interactive_button_action action = interactive_button_action_up;
		if (0 == inputEvent.compare(RPC_INPUT_EVENT_KEY_DOWN))
		{
			action = interactive_button_action_down;
		}

		inputData.buttonData.action = action;
	}
	else if (0 == inputEvent.compare(RPC_INPUT_EVENT_MOUSE_DOWN) ||
		0 == inputEvent.compare(RPC_INPUT_EVENT_MOUSE_UP))
	{
		inputData.type = input_type_click;
		interactive_button_action action = interactive_button_action_up;
		if (0 == inputEvent.compare(RPC_INPUT_EVENT_MOUSE_DOWN))
		{
			action = interactive_button_action_down;
		}

		inputData.buttonData.action = action;

		if (input.HasMember(RPC_INPUT_EVENT_MOVE_X))
		{
			inputData.coordinateData.x = input[RPC_INPUT_EVENT_MOVE_X].GetFloat();
		}
		if (input.HasMember(RPC_INPUT_EVENT_MOVE_Y))
		{
			inputData.coordinateData.y = input[RPC_INPUT_EVENT_MOVE_Y].GetFloat();
		}
	}
	else
	{
		inputData.type = input_type_custom;
	}

	session.onInput(session.callerContext, &session, &inputData);

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

int Chat::handle_participants_change(chat_session_internal& session, rapidjson::Document& doc, chat_participant_action action)
{
	if (!doc.HasMember(RPC_PARAMS) || !doc[RPC_PARAMS].HasMember(RPC_PARAM_PARTICIPANTS))
	{
		return MIXER_ERROR_UNRECOGNIZED_DATA_FORMAT;
	}

	rapidjson::Value& participants = doc[RPC_PARAMS][RPC_PARAM_PARTICIPANTS];
	for (auto itr = participants.Begin(); itr != participants.End(); ++itr)
	{
		chat_participant participant;
		parse_participant(*itr, participant);

		switch (action)
		{
		case participant_join_c:
		case participant_update_c:
		{
			std::shared_ptr<rapidjson::Document> participantDoc(std::make_shared<rapidjson::Document>());
			participantDoc->CopyFrom(*itr, participantDoc->GetAllocator());
			session.participants[participant.id] = participantDoc;
			break;
		}
		case participant_leave_c:
		default:
		{
			session.participants.erase(participant.id);
			break;
		}
		}

		if (session.onParticipantsChanged)
		{
			session.onParticipantsChanged(session.callerContext, &session, action, &participant);
		}
	}

	return MIXER_OK;
}

int Chat::handle_participants_join(chat_session_internal& session, rapidjson::Document& doc)
{
	return handle_participants_change(session, doc, participant_join_c);
}

int Chat::handle_participants_leave(chat_session_internal& session, rapidjson::Document& doc)
{
	return handle_participants_change(session, doc, participant_leave_c);
}

int Chat::handle_participants_update(chat_session_internal& session, rapidjson::Document& doc)
{
	return handle_participants_change(session, doc, participant_update_c);
}

int Chat::handle_ready(chat_session_internal& session, rapidjson::Document& doc)
{
	if (!doc.HasMember(RPC_PARAMS) || !doc[RPC_PARAMS].HasMember(RPC_PARAM_IS_READY))
	{
		return MIXER_ERROR_UNRECOGNIZED_DATA_FORMAT;
	}

	bool isReady = doc[RPC_PARAMS][RPC_PARAM_IS_READY].GetBool();
	// Only change state and notify if the ready state is different.
	if (isReady && chat_ready != session.state || !isReady && chat_connected != session.state)
	{
		chat_state previousState = session.state;
		session.state = isReady ? chat_ready : chat_connected;
		if (session.onStateChanged)
		{
			session.onStateChanged(session.callerContext, &session, previousState, session.state);
		}
	}

	return MIXER_OK;
}

int Chat::handle_group_changed(chat_session_internal& session, rapidjson::Document& doc)
{
	(doc);
	return cache_groups(session);
}

int Chat::handle_scene_changed(chat_session_internal& session, rapidjson::Document& doc)
{
	(doc);
	return cache_scenes(session);
}

struct user_object
{
	int userid;
	int channelid;
};

user_object getUserInfo(chat_session_internal& session)
{
	user_object result;
	result.userid = -1;
	result.channelid = -1;

	mixer_internal::http_response response;
	static std::string hostsUri = "https://mixer.com/api/v1/users/current";
	mixer_internal::http_headers headers;
	//headers.emplace("Content-Type", "application/json");
	headers.emplace("Authorization", session.m_auth->getAuthToken());

	// Critical Section: Http request.
	{
		std::unique_lock<std::mutex> httpLock(session.httpMutex);
		session.http->make_request(hostsUri, "GET", &headers, "", response);
	}

	if (200 != response.statusCode)
	{
		std::string errorMessage = "Failed to acquire chat access.";
		DnDPanel::Logger::Error(std::to_string(response.statusCode) + " " + errorMessage);
		session.enqueue_incoming_event(std::make_shared<error_event>(chat_error(MIXER_ERROR_NO_HOST, std::move(errorMessage))));

		return result;
	}

	std::string body = response.body.c_str();
	std::string anchor1 = "\"description\":\"";
	int anchor1Start = body.find(anchor1) + anchor1.length();
	body.erase(anchor1Start, body.find("\",\"typeId\"") - anchor1Start);
	rapidjson::Document resultDoc;
	if (resultDoc.Parse(body).HasParseError())
	{
		
		DnDPanel::Logger::Error("Error parsing :" + body);
		return result;
	}

	result.userid = resultDoc["id"].GetInt();
	result.channelid = resultDoc["channel"]["id"].GetInt();

	return result;
}
int Chat::handle_welcome_event(chat_session_internal& session, rapidjson::Document& doc)
{
	(doc);

	std::string methodJson = mixer_internal::jsonStringify(doc);

	std::shared_ptr<rapidjson::Document> mydoc(std::make_shared<rapidjson::Document>());
	mydoc->SetObject();
	rapidjson::Document::AllocatorType& allocator = mydoc->GetAllocator();

	mydoc->AddMember("type", "method", allocator);
	mydoc->AddMember("method", "auth", allocator);
	// Get the parameters from the caller.	
	
	rapidjson::Value params(rapidjson::kArrayType);

	user_object uo = getUserInfo(session);
	Value channelId(session.chatToConnect);
	Value userId(uo.userid);

	mixer_internal::http_response response;
	static std::string hostsUri = "https://mixer.com/api/v1/chats/" + std::to_string(uo.channelid);
	mixer_internal::http_headers headers;
	headers.emplace("Content-Type", "application/json");
	headers.emplace("Authorization", session.m_auth->getAuthToken());

	// Critical Section: Http request.
	{
		std::unique_lock<std::mutex> httpLock(session.httpMutex);
		RETURN_IF_FAILED(session.http->make_request(hostsUri, "GET", &headers, "", response));
	}

	if (200 != response.statusCode)
	{
		std::string errorMessage = "Failed to acquire chat access.";
		DnDPanel::Logger::Error(std::to_string(response.statusCode) + " " + errorMessage);
		session.enqueue_incoming_event(std::make_shared<error_event>(chat_error(MIXER_ERROR_NO_HOST, std::move(errorMessage))));

		return MIXER_ERROR_NO_HOST;
	}

	rapidjson::Document resultDoc;
	if (resultDoc.Parse(response.body.c_str()).HasParseError())
	{
		return MIXER_ERROR_JSON_PARSE;
	}

	Value auth(resultDoc["authkey"].GetString(), allocator);

	DnDPanel::Logger::Info(std::string("auth key:") + resultDoc["authkey"].GetString());

	params.PushBack(channelId, allocator);
	params.PushBack(userId, allocator);
	params.PushBack(auth, allocator);

	DnDPanel::Logger::Info(std::string("arguments for channel sign on: ") + mixer_internal::jsonStringify(params));

	mydoc->AddMember("arguments", params, allocator);
	mydoc->AddMember("id", "0", allocator);
	

	//DnDPanel::Logger::Info(std::string("Queueing method for sign on: ") + mixer_internal::jsonStringify(*mydoc));


	std::string packet = mixer_internal::jsonStringify(*mydoc);
	//DnDPanel::Logger::Info("Sending websocket message: " + packet);

	// Critical Section: Only one thread may send a websocket message at a time.
	int err = 0;
	{
		std::unique_lock<std::mutex> sendLock(session.websocketMutex);
		err = session.ws->send(packet);
	}
	return 0;
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

	RETURN_IF_FAILED(Chat::update_control_pointers(session, sceneId));

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

	RETURN_IF_FAILED(update_control_pointers(session, sceneId));

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

int Chat::handle_control_changed(chat_session_internal& session, rapidjson::Document& doc)
{
	if (!doc.HasMember(RPC_PARAMS)
		|| !doc[RPC_PARAMS].HasMember(RPC_PARAM_CONTROLS) || !doc[RPC_PARAMS][RPC_PARAM_CONTROLS].IsArray()
		|| !doc[RPC_PARAMS].HasMember(RPC_SCENE_ID) || !doc[RPC_PARAMS][RPC_SCENE_ID].IsString())
	{
		return MIXER_ERROR_UNRECOGNIZED_DATA_FORMAT;
	}

	const char * sceneId = doc[RPC_PARAMS][RPC_SCENE_ID].GetString();
	rapidjson::Value& controls = doc[RPC_PARAMS][RPC_PARAM_CONTROLS];
	for (auto itr = controls.Begin(); itr != controls.End(); ++itr)
	{
		chat_control control;
		memset(&control, 0, sizeof(chat_control));
		parse_control(*itr, control);

		chat_control_event eventType = chat_control_updated;
		if (0 == strcmp(doc[RPC_METHOD].GetString(), RPC_METHOD_ON_CONTROL_UPDATE))
		{
			RETURN_IF_FAILED(update_cached_control(session, control, *itr));
		}
		else if (0 == strcmp(doc[RPC_METHOD].GetString(), RPC_METHOD_ON_CONTROL_CREATE))
		{
			eventType = chat_control_created;
			RETURN_IF_FAILED(cache_new_control(session, sceneId, control, *itr));
		}
		else if (0 == strcmp(doc[RPC_METHOD].GetString(), RPC_METHOD_ON_CONTROL_DELETE))
		{
			eventType = chat_control_deleted;
			RETURN_IF_FAILED(delete_cached_control(session, sceneId, control));
		}
		else
		{
			return MIXER_ERROR_UNKNOWN_METHOD;
		}

		if (session.onControlChanged)
		{
			session.onControlChanged(session.callerContext, &session, eventType, &control);
		}
	}

	return MIXER_OK;
}

// Wrapper function uses a global to remember the object:
BotPtr chatBotSaved;

int chat_bot_message_wrapper(chat_session_internal& csi, rapidjson::Document& d)
{
	return chatBotSaved->handle_chat_message(csi, d);
}

int chat_bot_reply_wrapper(chat_session_internal& csi, rapidjson::Document& d)
{
	return chatBotSaved->handle_reply(csi, d);
}

int chat_bot_user_join_wrapper(chat_session_internal& csi, rapidjson::Document& d)
{
	return chatBotSaved->handle_user_join(csi, d);
}

int chat_bot_user_leave_wrapper(chat_session_internal& csi, rapidjson::Document& d)
{
	return chatBotSaved->handle_user_leave(csi, d);
}

void Chat::register_method_handlers(chat_session_internal& session, BotPtr chatBot)
{
	chatBotSaved = chatBot;

	session.methodHandlers.emplace(RPC_METHOD_HELLO, handle_hello);
	session.methodHandlers.emplace(RPC_METHOD_ON_READY_CHANGED, handle_ready);
	session.methodHandlers.emplace(RPC_METHOD_ON_INPUT, handle_input);
	session.methodHandlers.emplace(RPC_METHOD_ON_PARTICIPANT_JOIN, handle_participants_join);
	session.methodHandlers.emplace(RPC_METHOD_ON_PARTICIPANT_LEAVE, handle_participants_leave);
	session.methodHandlers.emplace(RPC_METHOD_ON_PARTICIPANT_UPDATE, handle_participants_update);
	session.methodHandlers.emplace(RPC_METHOD_ON_GROUP_UPDATE, handle_group_changed);
	session.methodHandlers.emplace(RPC_METHOD_ON_GROUP_CREATE, handle_group_changed);
	session.methodHandlers.emplace(RPC_METHOD_ON_CONTROL_UPDATE, handle_control_changed);
	session.methodHandlers.emplace(RPC_METHOD_UPDATE_SCENES, handle_scene_changed);

	session.methodHandlers.emplace(RPC_EVENT_WELCOME_EVENT, handle_welcome_event);
	session.methodHandlers.emplace(RPC_EVENT_CHAT_MESSAGE, chat_bot_message_wrapper);
	session.methodHandlers.emplace(RPC_EVENT_REPLY, chat_bot_reply_wrapper);
	session.methodHandlers.emplace(RPC_EVENT_USER_JOIN, chat_bot_user_join_wrapper);
	session.methodHandlers.emplace(RPC_EVENT_USER_LEAVE, chat_bot_user_leave_wrapper);
}

int Chat::chat_open_session(chat_session* sessionPtr, BotPtr chatBot)
{
	if (nullptr == sessionPtr)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	std::auto_ptr<chat_session_internal> session(new chat_session_internal());

	// Register method handlers
	register_method_handlers(*session, chatBot);

	// Initialize Http and Websocket clients
	session->http = mixer_internal::http_factory::make_http_client();
	session->ws = mixer_internal::websocket_factory::make_websocket();

	*sessionPtr = session.release();
	return MIXER_OK;
}

int Chat::get_interactive_hosts(chat_session_internal& session, std::vector<std::string>& interactiveHosts)
{
	DnDPanel::Logger::Info("Retrieving interactive hosts.");
	mixer_internal::http_response response;
	static std::string hostsUri = "https://mixer.com/api/v1/interactive/hosts";
	// Critical Section: Http request.
	{
		std::unique_lock<std::mutex> httpLock(session.httpMutex);
		RETURN_IF_FAILED(session.http->make_request(hostsUri, "GET", nullptr, "", response));
	}

	if (200 != response.statusCode)
	{
		std::string errorMessage = "Failed to acquire interactive host servers.";
		DnDPanel::Logger::Error(std::to_string(response.statusCode) + " " + errorMessage);
		session.enqueue_incoming_event(std::make_shared<error_event>(chat_error(MIXER_ERROR_NO_HOST, std::move(errorMessage))));

		return MIXER_ERROR_NO_HOST;
	}

	rapidjson::Document doc;
	if (doc.Parse(response.body.c_str()).HasParseError() || !doc.IsArray())
	{
		return MIXER_ERROR_JSON_PARSE;
	}

	for (auto itr = doc.Begin(); itr != doc.End(); ++itr)
	{
		auto addressItr = itr->FindMember("address");
		if (addressItr != itr->MemberEnd())
		{
			interactiveHosts.push_back(addressItr->value.GetString());
			DnDPanel::Logger::Info("Host found: " + std::string(addressItr->value.GetString(), addressItr->value.GetStringLength()));
		}
	}

	return MIXER_OK;
}

Chat::state_change_event::state_change_event(chat_state currentState) : chat_event_internal(chat_event_type_state_change), currentState(currentState) {}

int Chat::chat_connect(chat_session session, const char* auth, const char* versionId, const char* shareCode, bool setReady)
{
	if (nullptr == auth || nullptr == versionId)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	// Validate parameters
	if (0 == strlen(auth) || 0 == strlen(versionId))
	{
		return MIXER_ERROR_INVALID_VERSION_ID;
	}

	chat_session_internal* sessionInternal = reinterpret_cast<chat_session_internal*>(session);

	if (interactive_disconnected != sessionInternal->state)
	{
		return MIXER_ERROR_INVALID_STATE;
	}

	sessionInternal->isReady = setReady;
	sessionInternal->authorization = auth;
	sessionInternal->versionId = versionId;
	sessionInternal->shareCode = shareCode;

	sessionInternal->state = chat_connecting;
	if (sessionInternal->onStateChanged)
	{
		sessionInternal->onStateChanged(sessionInternal->callerContext, sessionInternal, chat_disconnected, sessionInternal->state);
		if (sessionInternal->shutdownRequested)
		{
			return MIXER_ERROR_CANCELLED;
		}
	}

	// Create thread to open websocket and receive messages.
	sessionInternal->incomingThread = std::thread(std::bind(&chat_session_internal::run_incoming_thread, sessionInternal));

	// Create thread to send messages over the open websocket.
	sessionInternal->outgoingThread = std::thread(std::bind(&chat_session_internal::run_outgoing_thread, sessionInternal));

	return MIXER_OK;
}

int Chat::route_method(chat_session_internal& session, rapidjson::Document& doc)
{
	std::string method = doc[RPC_METHOD].GetString();
	auto itr = session.methodHandlers.find(method);
	if (itr != session.methodHandlers.end())
	{
		return itr->second(session, doc);
	}
	else
	{
		DnDPanel::Logger::Info("Unhandled method type: " + method);
		if (session.onUnhandledMethod)
		{
			std::string methodJson = mixer_internal::jsonStringify(doc);
			session.onUnhandledMethod(session.callerContext, &session, methodJson.c_str(), methodJson.length());
		}
	}

	return MIXER_OK;
}

int Chat::route_event(chat_session_internal& session, rapidjson::Document& doc)
{
	std::string method = doc[RPC_EVENT].GetString();
	auto itr = session.methodHandlers.find(method);
	if (itr != session.methodHandlers.end())
	{
		return itr->second(session, doc);
	}
	else
	{
		DnDPanel::Logger::Info("Unhandled method type: " + method);
		if (session.onUnhandledMethod)
		{
			std::string methodJson = mixer_internal::jsonStringify(doc);
			session.onUnhandledMethod(session.callerContext, &session, methodJson.c_str(), methodJson.length());
		}
	}

	return MIXER_OK;
}

int Chat::chat_run(chat_session session, unsigned int maxEventsToProcess)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	if (0 == maxEventsToProcess)
	{
		return MIXER_OK;
	}

	chat_session_internal* sessionInternal = reinterpret_cast<chat_session_internal*>(session);

	if (sessionInternal->shutdownRequested)
	{
		return MIXER_ERROR_CANCELLED;
	}

	// Create a local queue and populate it with the top elements from the incoming event queue to minimize locking time.
	chat_event_queue processingQueue;
	{
		std::lock_guard<std::mutex> incomingLock(sessionInternal->incomingMutex);
		for (unsigned int i = 0; i < maxEventsToProcess && i < sessionInternal->incomingEvents.size(); ++i)
		{
			processingQueue.emplace(std::move(sessionInternal->incomingEvents.top()));
			sessionInternal->incomingEvents.pop();
		}
	}

	// Process all the events in the local queue.
	while (!processingQueue.empty())
	{
		auto ev = processingQueue.top();
		switch (ev->type)
		{
		case chat_event_type_error:
		{
			DnDPanel::Logger::Info("chat_event_type_error");
			auto errorEvent = reinterpret_cast<std::shared_ptr<error_event>&>(ev);
			if (sessionInternal->onError)
			{
				sessionInternal->onError(sessionInternal->callerContext, sessionInternal, errorEvent->error.first, errorEvent->error.second.c_str(), errorEvent->error.second.length());
				if (sessionInternal->shutdownRequested)
				{
					return MIXER_OK;
				}
			}
			break;
		}
		case chat_event_type_state_change:
		{
			DnDPanel::Logger::Info("chat_event_type_state_change");
			auto stateChangeEvent = reinterpret_cast<std::shared_ptr<state_change_event>&>(ev);
			chat_state previousState = sessionInternal->state;
			sessionInternal->state = stateChangeEvent->currentState;
			if (sessionInternal->onStateChanged)
			{
				sessionInternal->onStateChanged(sessionInternal->callerContext, sessionInternal, previousState, sessionInternal->state);
			}
			break;
		}
		case chat_event_type_rpc_reply:
		{
			DnDPanel::Logger::Info("chat_event_type_rpc_reply");
			auto replyEvent = reinterpret_cast<std::shared_ptr<rpc_reply_event>&>(ev);
			replyEvent->replyHandler(*sessionInternal, *replyEvent->replyJson);
			break;
		}
		case chat_event_type_http_response:
		{
			DnDPanel::Logger::Info("chat_event_type_http_response");
			auto httpResponseEvent = reinterpret_cast<std::shared_ptr<http_response_event>&>(ev);
			httpResponseEvent->responseHandler(httpResponseEvent->response);
			break;
		}
		case chat_event_type_rpc_method:
		{
			DnDPanel::Logger::Info("chat_event_type_rpc_method");
			auto rpcMethodEvent = reinterpret_cast<std::shared_ptr<rpc_method_event>&>(ev);
			if (rpcMethodEvent->methodJson->HasMember(RPC_SEQUENCE))
			{
				sessionInternal->sequenceId = (*rpcMethodEvent->methodJson)[RPC_SEQUENCE].GetInt();
			}

			RETURN_IF_FAILED(route_method(*sessionInternal, *rpcMethodEvent->methodJson));
			break;
		}
		case chat_event_type_rpc_event:
		{
			auto rpcMethodEvent = reinterpret_cast<std::shared_ptr<rpc_method_event>&>(ev);
			if (rpcMethodEvent->methodJson->HasMember(RPC_SEQUENCE))
			{
				sessionInternal->sequenceId = (*rpcMethodEvent->methodJson)[RPC_SEQUENCE].GetInt();
			}

			RETURN_IF_FAILED(route_event(*sessionInternal, *rpcMethodEvent->methodJson));
			break;
		}
		default:
			DnDPanel::Logger::Info("default");
			break;
		}

		processingQueue.pop();
		if (sessionInternal->shutdownRequested)
		{
			return MIXER_OK;
		}
	}

	return MIXER_OK;
}

void Chat::chat_close_session(chat_session session)
{
	if (nullptr != session)
	{
		chat_session_internal* sessionInternal = reinterpret_cast<chat_session_internal*>(session);

		// Mark the session as inactive and close the websocket. This will notify any functions in flight to exit at their earliest convenience.
		sessionInternal->shutdownRequested = true;
		if (nullptr != sessionInternal->ws.get())
		{
			sessionInternal->ws->close();
		}

		// Notify the outgoing websocket thread to shutdown.
		{
			std::unique_lock<std::mutex> outgoingLock(sessionInternal->outgoingMutex);
			sessionInternal->outgoingCV.notify_all();
		}

		// Wait for both threads to terminate.
		if (sessionInternal->incomingThread.joinable())
		{
			sessionInternal->incomingThread.join();
		}
		if (sessionInternal->outgoingThread.joinable())
		{
			sessionInternal->outgoingThread.join();
		}

		// Clean up the session memory.
		delete sessionInternal;
	}
}