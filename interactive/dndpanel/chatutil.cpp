#include "chatutil.h"
#include "config.h"
#include "chatbot.h"

#include <windows.h>
#include <shellapi.h>

#undef GetObject

using namespace ChatUtil;
using namespace ChatBot;


chat_session_internal::chat_session_internal()
	: callerContext(nullptr), isReady(false), state(chat_disconnected), shutdownRequested(false), packetId(0),
	sequenceId(0), wsOpen(false), onInput(nullptr), onError(nullptr), onStateChanged(nullptr), onParticipantsChanged(nullptr),
	onUnhandledMethod(nullptr), onControlChanged(nullptr), onTransactionComplete(nullptr), serverTimeOffsetMs(0), serverTimeOffsetCalculated(false), scenesCached(false), groupsCached(false),
	getTimeRequestId(0xffffffff)
{
	scenesRoot.SetObject();
}

ChatUtil::chat_event_internal::chat_event_internal(chat_event_type type) : type(type) {}

#define JSON_CODE "code"
#define JSON_HANDLE "handle"

int chat_auth_get_short_code(const char* clientId, const char* clientSecret, char* shortCode, size_t* shortCodeLength, char* shortCodeHandle, size_t* shortCodeHandleLength)
{
	if (nullptr == clientId || nullptr == shortCode || nullptr == shortCodeLength || nullptr == shortCodeHandle || nullptr == shortCodeHandleLength)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	mixer_internal::http_response response;
	std::string oauthCodeUrl = "https://mixer.com/api/v1/oauth/shortcode";

	// Construct the json body
	std::string jsonBody;
	if (nullptr == clientSecret)
	{
		jsonBody = std::string("{ \"client_id\": \"") + clientId + "\", \"scope\": \"chat:chat chat:connect\" }";
	}
	else
	{
		jsonBody = std::string("{ \"client_id\": \"") + clientId + "\", \"client_secret\": \"" + clientSecret + "\", \"scope\": \"interactive:robot:self\" }";
	}

	std::unique_ptr<mixer_internal::http_client> client = mixer_internal::http_factory::make_http_client();
	RETURN_IF_FAILED(client->make_request(oauthCodeUrl, "POST", nullptr, jsonBody, response));
	if (200 != response.statusCode)
	{
		return response.statusCode;
	}

	rapidjson::Document doc;
	if (doc.Parse(response.body.c_str()).HasParseError())
	{
		return MIXER_ERROR_JSON_PARSE;
	}

	std::string code = doc[JSON_CODE].GetString();
	std::string handle = doc[JSON_HANDLE].GetString();

	if (*shortCodeLength < code.length() + 1 ||
		*shortCodeHandleLength < handle.length() + 1)
	{
		*shortCodeLength = code.length() + 1;
		*shortCodeHandleLength = handle.length() + 1;
		return MIXER_ERROR_BUFFER_SIZE;
	}

	memcpy(shortCode, code.c_str(), code.length());
	shortCode[code.length()] = 0;
	*shortCodeLength = code.length() + 1;

	memcpy(shortCodeHandle, handle.c_str(), handle.length());
	shortCodeHandle[handle.length()] = 0;
	*shortCodeHandleLength = handle.length() + 1;
	return MIXER_OK;
}

int Auth::EnsureAuth(DnDPanel::DndConfigPtr config)
{
	int err = 0;

	this->authToken = std::string("");

	// Try to use the old token if it exists.
	do
	{
		// If we have a token, try to refresh.
		if (config->RefreshToken.length() > 0)
		{
			char newToken[1024];
			size_t newTokenLength = _countof(newToken);
			err = interactive_auth_refresh_token(config->ClientId.c_str(), nullptr, config->RefreshToken.c_str(), newToken, &newTokenLength);
			if (!err)
			{
				config->RefreshToken = std::string(newToken, newTokenLength);
				break;
			}
			else
			{
				// If failed clear the token
				config->RefreshToken = "";
			}
		}
		else
		{
			// Start a clean auth
			char shortCode[7];
			size_t shortCodeLength = _countof(shortCode);
			char shortCodeHandle[1024];
			size_t shortCodeHandleLength = _countof(shortCodeHandle);
			err = interactive_auth_get_short_code(CLIENT_ID, nullptr, shortCode, &shortCodeLength, shortCodeHandle, &shortCodeHandleLength);
			
			if (err) return err;

			// Pop the browser for the user to approve access.
			std::string authUrl = std::string("https://www.mixer.com/go?code=") + shortCode;
			ShellExecuteA(0, 0, authUrl.c_str(), nullptr, nullptr, SW_SHOW);

			// Wait for OAuth token response.
			char refreshTokenBuffer[1024];
			size_t refreshTokenLength = _countof(refreshTokenBuffer);
			err = interactive_auth_wait_short_code(CLIENT_ID, nullptr, shortCodeHandle, refreshTokenBuffer, &refreshTokenLength);
			if (err)
			{
				if (MIXER_ERROR_TIMED_OUT == err)
				{
					std::cout << "Authorization timed out, user did not approve access within the time limit." << std::endl;
				}
				else if (MIXER_ERROR_AUTH_DENIED == err)
				{
					std::cout << "User denied access." << std::endl;
				}

				return err;
			}

			// Cache the refresh token
			config->RefreshToken = std::string(refreshTokenBuffer, refreshTokenLength);
			break;
		}
	} while (config->RefreshToken.length() == 0);

	// Write the config.
	config->Write();

	// Extract the authorization header from the refresh token.
	char authBuffer[1024];
	size_t authBufferLength = _countof(authBuffer);
	err = interactive_auth_parse_refresh_token(config->RefreshToken.c_str(), authBuffer, &authBufferLength);
	if (err)
	{
		return err;
	}

	// Success!
	authToken = std::string(authBuffer, authBufferLength);
	return 0;
}

int Auth::EnsureAuth(DnDPanel::ChatConfigPtr config)
{
	int err = 0;

	this->authToken = std::string("");

	// Try to use the old token if it exists.
	do
	{
		// If we have a token, try to refresh.
		if (config->RefreshToken.length() > 0)
		{
			char newToken[1024];
			size_t newTokenLength = _countof(newToken);
			err = interactive_auth_refresh_token(config->ClientId.c_str(), nullptr, config->RefreshToken.c_str(), newToken, &newTokenLength);
			if (!err)
			{
				config->RefreshToken = std::string(newToken, newTokenLength);
				break;
			}
			else
			{
				// If failed clear the token
				config->RefreshToken = "";
			}
		}
		else
		{
			// Start a clean auth
			char shortCode[7];
			size_t shortCodeLength = _countof(shortCode);
			char shortCodeHandle[1024];
			size_t shortCodeHandleLength = _countof(shortCodeHandle);
			err = chat_auth_get_short_code(CLIENT_ID, nullptr, shortCode, &shortCodeLength, shortCodeHandle, &shortCodeHandleLength);
			
			if (err) return err;

			// Pop the browser for the user to approve access.
			std::string authUrl = std::string("https://www.mixer.com/go?code=") + shortCode;
			ShellExecuteA(0, 0, authUrl.c_str(), nullptr, nullptr, SW_SHOW);

			// Wait for OAuth token response.
			char refreshTokenBuffer[1024];
			size_t refreshTokenLength = _countof(refreshTokenBuffer);
			err = interactive_auth_wait_short_code(CLIENT_ID, nullptr, shortCodeHandle, refreshTokenBuffer, &refreshTokenLength);
			if (err)
			{
				if (MIXER_ERROR_TIMED_OUT == err)
				{
					std::cout << "Authorization timed out, user did not approve access within the time limit." << std::endl;
				}
				else if (MIXER_ERROR_AUTH_DENIED == err)
				{
					std::cout << "User denied access." << std::endl;
				}

				return err;
			}

			// Cache the refresh token
			config->RefreshToken = std::string(refreshTokenBuffer, refreshTokenLength);
			break;
		}
	} while (config->RefreshToken.length() == 0);

	// Write the config.
	config->Write();

	// Extract the authorization header from the refresh token.
	char authBuffer[1024];
	size_t authBufferLength = _countof(authBuffer);
	err = interactive_auth_parse_refresh_token(config->RefreshToken.c_str(), authBuffer, &authBufferLength);
	if (err)
	{
		return err;
	}

	// Success!
	authToken = std::string(authBuffer, authBufferLength);
	return 0;
}

int ChatUtil::update_control_pointers(chat_session_internal& session, const char* sceneId)
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

int ChatUtil::update_cached_control(chat_session_internal& session, chat_control& control, rapidjson::Value& controlJson)
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

int ChatUtil::create_method_json(chat_session_internal& session, const std::string& method, on_get_params getParams, bool discard, unsigned int* id, std::shared_ptr<rapidjson::Document>& methodDoc)
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
int ChatUtil::queue_method(chat_session_internal& session, const std::string& method, on_get_params getParams, method_handler_c onReply, const bool handleImmediately)
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

int ChatUtil::send_ready_message(chat_session_internal& session, bool ready)
{
	return queue_method(session, RPC_METHOD_READY, [&](rapidjson::Document::AllocatorType& allocator, rapidjson::Value& params)
	{
		params.AddMember(RPC_PARAM_IS_READY, ready, allocator);
	}, nullptr, false);
}

int ChatUtil::cache_scenes(chat_session_internal& session)
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

int ChatUtil::cache_groups(chat_session_internal& session)
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
int ChatUtil::bootstrap(chat_session_internal& session)
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

int ChatUtil::update_server_time_offset(chat_session_internal& session)
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

int ChatUtil::handle_hello(chat_session_internal& session, rapidjson::Document& doc)
{
	(doc);
	if (session.shutdownRequested)
	{
		return MIXER_OK;
	}

	return bootstrap(session);
}

int ChatUtil::handle_input(chat_session_internal& session, rapidjson::Document& doc)
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

void ChatUtil::parse_participant(rapidjson::Value& participantJson, chat_participant& participant)
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

int ChatUtil::handle_participants_change(chat_session_internal& session, rapidjson::Document& doc, chat_participant_action action)
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

int ChatUtil::handle_participants_join(chat_session_internal& session, rapidjson::Document& doc)
{
	return handle_participants_change(session, doc, participant_join_c);
}

int ChatUtil::handle_participants_leave(chat_session_internal& session, rapidjson::Document& doc)
{
	return handle_participants_change(session, doc, participant_leave_c);
}

int ChatUtil::handle_participants_update(chat_session_internal& session, rapidjson::Document& doc)
{
	return handle_participants_change(session, doc, participant_update_c);
}

int ChatUtil::handle_ready(chat_session_internal& session, rapidjson::Document& doc)
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

int ChatUtil::handle_group_changed(chat_session_internal& session, rapidjson::Document& doc)
{
	(doc);
	return cache_groups(session);
}

int ChatUtil::handle_scene_changed(chat_session_internal& session, rapidjson::Document& doc)
{
	(doc);
	return cache_scenes(session);
}

int ChatUtil::handle_welcome_event(chat_session_internal& session, rapidjson::Document& doc)
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
	Value channelId(248987);
	Value userId(354879);

	mixer_internal::http_response response;
	static std::string hostsUri = "https://mixer.com/api/v1/chats/248987";
	mixer_internal::http_headers headers;
	headers.emplace("Content-Type", "application/json");
	headers.emplace("Authorization", session.m_auth->authToken);

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

int ChatUtil::cache_new_control(chat_session_internal& session, const char* sceneId, chat_control& control, rapidjson::Value& controlJson)
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

	RETURN_IF_FAILED(ChatUtil::update_control_pointers(session, sceneId));

	return MIXER_OK;
}

int ChatUtil::delete_cached_control(chat_session_internal& session, const char* sceneId, chat_control& control)
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

void ChatUtil::parse_control(rapidjson::Value& controlJson, chat_control& control)
{
	control.id = controlJson[RPC_CONTROL_ID].GetString();
	control.idLength = controlJson[RPC_CONTROL_ID].GetStringLength();
	if (controlJson.HasMember(RPC_CONTROL_KIND))
	{
		control.kind = controlJson[RPC_CONTROL_KIND].GetString();
		control.kindLength = controlJson[RPC_CONTROL_KIND].GetStringLength();
	}
}

int ChatUtil::handle_control_changed(chat_session_internal& session, rapidjson::Document& doc)
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

void ChatUtil::register_method_handlers(chat_session_internal& session)
{
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
	session.methodHandlers.emplace(RPC_EVENT_CHAT_MESSAGE, handle_chat_message);
	session.methodHandlers.emplace(RPC_EVENT_REPLY, handle_reply);
	session.methodHandlers.emplace(RPC_EVENT_USER_JOIN, handle_user_join);
	session.methodHandlers.emplace(RPC_EVENT_USER_LEAVE, handle_user_leave);
}

int ChatUtil::chat_open_session(chat_session* sessionPtr)
{
	if (nullptr == sessionPtr)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	std::auto_ptr<chat_session_internal> session(new chat_session_internal());

	// Register method handlers
	register_method_handlers(*session);

	// Initialize Http and Websocket clients
	session->http = mixer_internal::http_factory::make_http_client();
	session->ws = mixer_internal::websocket_factory::make_websocket();

	*sessionPtr = session.release();
	return MIXER_OK;
}

int ChatUtil::get_interactive_hosts(chat_session_internal& session, std::vector<std::string>& interactiveHosts)
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

void ChatUtil::chat_session_internal::handle_ws_open(const mixer_internal::websocket& socket, const std::string& message)
{
	(socket);
	DnDPanel::Logger::Info("Websocket opened: " + message);
	// Notify the outgoing thread.
	this->wsOpen = true;
}

void ChatUtil::chat_session_internal::handle_ws_message(const mixer_internal::websocket& socket, const std::string& message)
{
	(socket);
	DnDPanel::Logger::Info("Websocket message received: " + message);
	if (this->shutdownRequested)
	{
		return;
	}

	// Parse the message to determine packet type.
	std::shared_ptr<rapidjson::Document> messageJson = std::make_shared<rapidjson::Document>();
	if (!messageJson->Parse(message.c_str(), message.length()).HasParseError())
	{
		if (!messageJson->HasMember(RPC_TYPE))
		{
			// Message does not conform to protocol, ignore it.
			DnDPanel::Logger::Info("Incoming RPC packet missing type parameter.");
			return;
		}

		std::string type = (*messageJson)[RPC_TYPE].GetString();
		if (0 == type.compare(RPC_METHOD))
		{
			this->enqueue_incoming_event(std::make_shared<rpc_method_event>(std::move(messageJson)));
		}
		else if (0 == type.compare(RPC_REPLY))
		{
			std::string mj = mixer_internal::jsonStringify(*messageJson);
			if ((*messageJson)[RPC_ID].IsString())
			{
				unsigned int id = std::stoi((*messageJson)[RPC_ID].GetString());
				method_handler_c handlerFunc = nullptr;
				bool executeImmediately = false;
				// Critical Section: Check if there is a registered reply handler and if it's marked for immediate execution.
				{
					std::unique_lock<std::mutex> incomingLock(this->incomingMutex);
					auto replyHandlerItr = this->replyHandlersById.find(id);
					if (replyHandlerItr != this->replyHandlersById.end())
					{
						executeImmediately = replyHandlerItr->second.first;
						handlerFunc.swap(replyHandlerItr->second.second);
						this->replyHandlersById.erase(replyHandlerItr);
					}
				}

				if (nullptr != handlerFunc)
				{
					if (executeImmediately)
					{
						handlerFunc(*this, *messageJson);
					}
					else
					{
						this->enqueue_incoming_event(std::make_shared<rpc_reply_event>(id, std::move(messageJson), handlerFunc));
					}
				}
			}
			
		}
		else if (0 == type.compare(RPC_EVENT))
		{
			this->enqueue_incoming_event(std::make_shared<rpc_event_event>(std::move(messageJson)));
		}
		else
		{
			DnDPanel::Logger::Error("Recived unknown type of message(" + type + ")");
		}
	}
	else
	{
		DnDPanel::Logger::Error("Failed to parse websocket message: " + message);
	}
}

void ChatUtil::chat_session_internal::handle_ws_close(const mixer_internal::websocket& socket, const unsigned short code, const std::string& message)
{
	(socket);
	DnDPanel::Logger::Info("Websocket closed: " + message + " (" + std::to_string(code) + ")");
}

void ChatUtil::chat_session_internal::run_incoming_thread()
{
	auto onWsOpen = std::bind(&chat_session_internal::handle_ws_open, this, std::placeholders::_1, std::placeholders::_2);
	auto onWsMessage = std::bind(&chat_session_internal::handle_ws_message, this, std::placeholders::_1, std::placeholders::_2);
	auto onWsClose = std::bind(&chat_session_internal::handle_ws_close, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

	// Interactive hosts in retry order.
	std::vector<std::string> hosts = { "wss://chat4-dal.mixer.com:443", "wss://chat1-dal.mixer.com:443", "wss://chat2-dal.mixer.com:443" };
	std::vector<std::string>::iterator hostItr;
	static unsigned int connectionRetryFrequency = DEFAULT_CONNECTION_RETRY_FREQUENCY_S;

	while (!shutdownRequested)
	{
		hostItr = hosts.begin();


		// Connect long running websocket.
		DnDPanel::Logger::Info("Connecting to websocket: " + *hostItr);
		this->ws->add_header("Content-Type", "application/json");
		this->ws->add_header("Authorization", "Bearer " + this->authorization);

		int err = this->ws->open(*hostItr, onWsOpen, onWsMessage, nullptr, onWsClose);
		if (this->shutdownRequested)
		{
			break;
		}

		if (err)
		{
			std::string errorMessage;
			if (!this->wsOpen)
			{
				errorMessage = "Failed to open websocket: " + *hostItr;
				DnDPanel::Logger::Error(std::to_string(err) + " " + errorMessage);
				enqueue_incoming_event(std::make_shared<error_event>(chat_error(MIXER_ERROR_WS_CONNECT_FAILED, std::move(errorMessage))));
			}
			else
			{
				this->wsOpen = false;
				errorMessage = "Lost connection to websocket: " + *hostItr;
				// Since there was a successful connection, reset the connection retry frequency and hosts.
				connectionRetryFrequency = DEFAULT_CONNECTION_RETRY_FREQUENCY_S;
				enqueue_incoming_event(std::make_shared<error_event>(chat_error(MIXER_ERROR_WS_CLOSED, errorMessage)));

				// When the websocket closes, interactive state is fully reset. Clear any pending methods.
				// Critical Section: Clear websocket methods.
				{
					std::lock_guard<std::mutex> outgoingLock(this->outgoingMutex);
					std::queue<std::shared_ptr<chat_event_internal>> cleanOutgoingEvents;
					while (!this->outgoingEvents.empty())
					{
						auto ev = this->outgoingEvents.front();
						if (ev->type != chat_event_type_rpc_method)
						{
							cleanOutgoingEvents.emplace(std::move(ev));
						}
						this->outgoingEvents.pop();
					}

					if (!cleanOutgoingEvents.empty())
					{
						this->outgoingEvents.swap(cleanOutgoingEvents);
					}
				}

				// Reset bootstraps
				this->serverTimeOffsetCalculated = false;
				this->groupsCached = false;
				this->scenesCached = false;

				enqueue_incoming_event(std::make_shared<state_change_event>(chat_connecting));
			}
		}

		++hostItr;

		// Once all the hosts have been tried, clear it and get a new list of hosts.
		if (hostItr == hosts.end())
		{
			hosts = {};
			std::this_thread::sleep_for(std::chrono::seconds(connectionRetryFrequency));
			connectionRetryFrequency = std::min<unsigned int>(MAX_CONNECTION_RETRY_FREQUENCY_S, connectionRetryFrequency *= 2);
		}
	}
}

ChatUtil::state_change_event::state_change_event(chat_state currentState) : chat_event_internal(chat_event_type_state_change), currentState(currentState) {}

void ChatUtil::chat_session_internal::run_outgoing_thread()
{
	std::queue<std::shared_ptr<chat_event_internal>> processingEvents;
	// Run this thread continuously until shutdown is requested.
	while (!shutdownRequested)
	{
		if (!processingEvents.empty())
		{
			// If the processing queue still contains messages after a loop executes, there must have been an error. Throttle retries by sleeping this thread whenever this happens.
			std::this_thread::sleep_for(std::chrono::seconds(DEFAULT_CONNECTION_RETRY_FREQUENCY_S));
		}
		else
		{
			// Critical section: Check if there are any queued methods or requests that need to be sent.
			std::unique_lock<std::mutex> lock(outgoingMutex);
			if (this->outgoingEvents.empty())
			{
				outgoingCV.wait(lock);
				// Since this thread just woke up, check if it has been signalled to stop.
				if (shutdownRequested)
				{
					break;
				}
			}

			processingEvents.swap(this->outgoingEvents);
		}

		// Process all http requests.
		while (!processingEvents.empty() && !shutdownRequested)
		{
			auto ev = processingEvents.front();
			switch (ev->type)
			{
			case chat_event_type_http_request:
			{
				auto request = reinterpret_cast<std::shared_ptr<http_request_event>&>(ev);
				mixer_internal::http_response response;
				int err = http->make_request(request->uri, request->verb, request->headers.empty() ? nullptr : &request->headers, request->body, response);
				if (err)
				{
					std::string errorMessage = "Failed to '" + request->verb + "' to " + request->uri;
					DnDPanel::Logger::Error(std::to_string(err) + " " + errorMessage);
					enqueue_incoming_event(std::make_shared<error_event>(interactive_error(MIXER_ERROR_HTTP, std::move(errorMessage))));
				}
				else
				{
					// This request was successfully sent, remove it from the queue.
					processingEvents.pop();

					DnDPanel::Logger::Info("HTTP response received: (" + std::to_string(response.statusCode) + ") " + response.body);
					// Critical Section: Find the response handler for this request.
					http_response_handler handler = nullptr;
					{
						std::unique_lock<std::mutex> incomingLock(this->incomingMutex);
						auto responseHandlerItr = this->httpResponseHandlers.find(request->packetId);
						if (responseHandlerItr != this->httpResponseHandlers.end())
						{
							handler = std::move(responseHandlerItr->second);
							this->httpResponseHandlers.erase(responseHandlerItr);
						}
					}

					if (nullptr != handler)
					{
						enqueue_incoming_event(std::make_shared<http_response_event>(std::move(response), handler));
					}
				}

				break;
			}
			case chat_event_type_rpc_method:
			{
				if (!this->wsOpen)
				{
					// If the websocket is not open, skip processing this event.
					break;
				}

				auto methodEvent = reinterpret_cast<std::shared_ptr<rpc_method_event>&>(ev);
				std::string packet = mixer_internal::jsonStringify(*(methodEvent->methodJson));
				//DnDPanel::Logger::Info("Sending websocket message: " + packet);

				// Critical Section: Only one thread may send a websocket message at a time.
				int err = 0;
				{
					std::unique_lock<std::mutex> sendLock(this->websocketMutex);
					err = this->ws->send(packet);
				}

				if (err)
				{
					std::string errorMessage = "Failed to send websocket message.";
					DnDPanel::Logger::Error(std::to_string(err) + " " + errorMessage);
					enqueue_incoming_event(std::make_shared<error_event>(interactive_error(MIXER_ERROR_WS_SEND_FAILED, std::move(errorMessage))));

					// An error here implies that the connection is broken.
					// Break out of the websocket method loop so that http requests are not starved and retry.
					break;
				}
				else
				{
					// Method sent successfully.
					processingEvents.pop();
				}
				break;
			}
			default:
			{
				assert(false);
				break;
			}
			}
		}
	}
}

int ChatUtil::chat_connect(chat_session session, const char* auth, const char* versionId, const char* shareCode, bool setReady)
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

int ChatUtil::route_method(chat_session_internal& session, rapidjson::Document& doc)
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

int ChatUtil::route_event(chat_session_internal& session, rapidjson::Document& doc)
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

int ChatUtil::chat_run(chat_session session, unsigned int maxEventsToProcess)
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
			DnDPanel::Logger::Info("chat_event_type_rpc_event");
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

void ChatUtil::chat_close_session(chat_session session)
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