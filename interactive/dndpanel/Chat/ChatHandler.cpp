#include "ChatHandler.h"

#include "internal/json.h"
#include "../dndpanel/chatutil.h"

using namespace Chat;
using namespace std;
using namespace rapidjson;

ChatHandler::ChatHandler()
	: shutdownRequested(false), 
	  wsOpen(false),  
	  serverTimeOffsetCalculated(false), 
	  scenesCached(false), 
	  groupsCached(false), 
	  packetId(0), 
	  sequenceId(0), 
	  state(chat_disconnected), 
	  onStateChanged(nullptr),
	  onUnhandledMethod(nullptr)
{
}

int ChatHandler::handle_chat_message(rapidjson::Document& doc)
{
	(doc);

	for (auto& v : doc["data"]["message"]["message"].GetArray())
	{
		std::string found_text = v.GetObject()["text"].GetString();
		std::string command = found_text.substr(0, found_text.find(" "));

		map<std::string, std::function<void(rapidjson::Document&)>>::iterator it = funcMap.find(command);
		std::function<void(rapidjson::Document&)> b3;
		if (it != funcMap.end())
		{
			//element found;
			b3 = it->second;
			b3(doc);
		}
	}

	return 0;
}

int ChatHandler::handle_reply(rapidjson::Document& doc)
{
	(doc);
	return 0;
}

int ChatHandler::handle_user_join( rapidjson::Document& doc)
{
	(doc);
	return 0;
}

int ChatHandler::handle_user_leave(rapidjson::Document& doc)
{
	(doc);
	return 0;
}



void ChatHandler::sendMessage(std::string message)
{
	std::shared_ptr<rapidjson::Document> doc(std::make_shared<rapidjson::Document>());
	doc->SetObject();
	rapidjson::Document::AllocatorType& allocator = doc->GetAllocator();


	doc->AddMember("type", "method", allocator);
	doc->AddMember("method", "msg", allocator);
	Value a(kArrayType);
	Value chatMessage(message, allocator);
	a.PushBack(chatMessage, allocator);
	doc->AddMember("arguments", a, allocator);
	doc->AddMember("id", message_id, allocator);
	message_id++;


	//DnDPanel::Logger::Info(std::string("Queueing method: ") + mixer_internal::jsonStringify(*doc));
	std::shared_ptr<rpc_method_event> methodEvent = std::make_shared<rpc_method_event>(std::move(doc));
	std::unique_lock<std::mutex> queueLock(outgoingMutex);
	outgoingEvents.emplace(methodEvent);
	outgoingCV.notify_one();
}

void ChatHandler::sendWhisper(std::string message, std::string target)
{
	std::shared_ptr<rapidjson::Document> doc(std::make_shared<rapidjson::Document>());
	doc->SetObject();
	rapidjson::Document::AllocatorType& allocator = doc->GetAllocator();


	doc->AddMember("type", "method", allocator);
	doc->AddMember("method", "whisper", allocator);
	Value a(kArrayType);
	Value chatMessage(message, allocator);
	Value chatTarget(target, allocator);
	a.PushBack(chatTarget, allocator);
	a.PushBack(chatMessage, allocator);
	doc->AddMember("arguments", a, allocator);
	doc->AddMember("id", message_id, allocator);
	message_id++;


	//DnDPanel::Logger::Info(std::string("Queueing method: ") + mixer_internal::jsonStringify(*doc));
	std::shared_ptr<rpc_method_event> methodEvent = std::make_shared<rpc_method_event>(std::move(doc));
	std::unique_lock<std::mutex> queueLock(outgoingMutex);
	outgoingEvents.emplace(methodEvent);
	outgoingCV.notify_one();
}

void ChatHandler::deleteMessage(std::string id)
{
	std::shared_ptr<rapidjson::Document> doc(std::make_shared<rapidjson::Document>());
	doc->SetObject();
	rapidjson::Document::AllocatorType& allocator = doc->GetAllocator();


	doc->AddMember("type", "method", allocator);
	doc->AddMember("method", "deleteMessage", allocator);
	Value a(kArrayType);
	Value chatMessage(id, allocator);
	a.PushBack(chatMessage, allocator);
	doc->AddMember("arguments", a, allocator);
	doc->AddMember("id", message_id, allocator);
	message_id++;


	//DnDPanel::Logger::Info(std::string("Queueing method: ") + mixer_internal::jsonStringify(*doc));
	std::shared_ptr<rpc_method_event> methodEvent = std::make_shared<rpc_method_event>(std::move(doc));
	std::unique_lock<std::mutex> queueLock(outgoingMutex);
	outgoingEvents.emplace(methodEvent);
	outgoingCV.notify_one();
}

void ChatHandler::run_outgoing_thread()
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
			if (outgoingEvents.empty())
			{
				outgoingCV.wait(lock);
				// Since this thread just woke up, check if it has been signalled to stop.
				if (shutdownRequested)
				{
					break;
				}
			}

			processingEvents.swap(outgoingEvents);
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
				if (!wsOpen)
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
					std::unique_lock<std::mutex> sendLock(websocketMutex);
					err = ws->send(packet);
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

void ChatHandler::run_incoming_thread()
{
	auto onWsOpen = std::bind(&ChatHandler::handle_ws_open, this, std::placeholders::_1, std::placeholders::_2);
	auto onWsMessage = std::bind(&ChatHandler::handle_ws_message, this, std::placeholders::_1, std::placeholders::_2);
	auto onWsClose = std::bind(&ChatHandler::handle_ws_close, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

	// Interactive hosts in retry order.
	std::vector<std::string> hosts = { "wss://chat4-dal.mixer.com:443", "wss://chat1-dal.mixer.com:443", "wss://chat2-dal.mixer.com:443" };
	std::vector<std::string>::iterator hostItr;
	static unsigned int connectionRetryFrequency = DEFAULT_CONNECTION_RETRY_FREQUENCY_S;

	while (!shutdownRequested)
	{
		hostItr = hosts.begin();


		// Connect long running websocket.
		DnDPanel::Logger::Info("Connecting to websocket: " + *hostItr);
		ws->add_header("Content-Type", "application/json");
		ws->add_header("Authorization", "Bearer " + authorization);

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
				serverTimeOffsetCalculated = false;
				groupsCached = false;
				scenesCached = false;

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

void ChatHandler::handle_ws_open(const mixer_internal::websocket& socket, const std::string& message)
{
	(socket);
	DnDPanel::Logger::Info("Websocket opened: " + message);
	// Notify the outgoing thread.
	this->wsOpen = true;
}

void ChatHandler::handle_ws_message(const mixer_internal::websocket& socket, const std::string& message)
{
	(socket);
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
						//handlerFunc(this, *messageJson);
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

void ChatHandler::handle_ws_close(const mixer_internal::websocket& socket, const unsigned short code, const std::string& message)
{
	(socket);
	DnDPanel::Logger::Info("Websocket closed: " + message + " (" + std::to_string(code) + ")");
}

int handle_welcome_event(ChatHandlerPtr chatHandler, rapidjson::Document& doc)
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

	user_object uo = chatHandler->getUserInfo();
	rapidjson::Value channelId(chatHandler->chatToConnect);
	rapidjson::Value userId(uo.userid);

	mixer_internal::http_response response;
	static std::string hostsUri = "https://mixer.com/api/v1/chats/" + std::to_string(uo.channelid);
	mixer_internal::http_headers headers;
	headers.emplace("Content-Type", "application/json");
	headers.emplace("Authorization", chatHandler->m_auth->getAuthToken());

	// Critical Section: Http request.
	{
		std::unique_lock<std::mutex> httpLock(chatHandler->httpMutex);
		RETURN_IF_FAILED(chatHandler->http->make_request(hostsUri, "GET", &headers, "", response));
	}

	if (200 != response.statusCode)
	{
		std::string errorMessage = "Failed to acquire chat access.";
		DnDPanel::Logger::Error(std::to_string(response.statusCode) + " " + errorMessage);
		chatHandler->enqueue_incoming_event(std::make_shared<error_event>(chat_error(MIXER_ERROR_NO_HOST, std::move(errorMessage))));

		return MIXER_ERROR_NO_HOST;
	}

	rapidjson::Document resultDoc;
	if (resultDoc.Parse(response.body.c_str()).HasParseError())
	{
		return MIXER_ERROR_JSON_PARSE;
	}

	rapidjson::Value auth(resultDoc["authkey"].GetString(), allocator);

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
		std::unique_lock<std::mutex> sendLock(chatHandler->websocketMutex);
		err = chatHandler->ws->send(packet);
	}
	return 0;
}

int chat_bot_message_wrapper(ChatHandlerPtr chatHandler, rapidjson::Document& d)
{
	return chatHandler->handle_chat_message(d);
}

int chat_bot_reply_wrapper(ChatHandlerPtr chatHandler, rapidjson::Document& d)
{
	return chatHandler->handle_reply(d);
}

int chat_bot_user_join_wrapper(ChatHandlerPtr chatHandler, rapidjson::Document& d)
{
	return chatHandler->handle_user_join(d);
}

int chat_bot_user_leave_wrapper(ChatHandlerPtr chatHandler, rapidjson::Document& d)
{
	return chatHandler->handle_user_leave(d);
}

void ChatHandler::register_method_handlers()
{
	//methodHandlers.emplace(RPC_METHOD_ON_READY_CHANGED, handle_ready);
	//methodHandlers.emplace(RPC_METHOD_ON_INPUT, handle_input);
	//methodHandlers.emplace(RPC_METHOD_ON_PARTICIPANT_UPDATE, handle_participants_update);

	methodHandlers.emplace(RPC_EVENT_WELCOME_EVENT, handle_welcome_event);
	methodHandlers.emplace(RPC_EVENT_CHAT_MESSAGE, chat_bot_message_wrapper);
	methodHandlers.emplace(RPC_EVENT_REPLY, chat_bot_reply_wrapper);
	methodHandlers.emplace(RPC_EVENT_USER_JOIN, chat_bot_user_join_wrapper);
	methodHandlers.emplace(RPC_EVENT_USER_LEAVE, chat_bot_user_leave_wrapper);
}

int ChatHandler::chat_connect(const char* auth, const char* versionId, const char* shareCode, bool setReady)
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

	if (interactive_disconnected != state)
	{
		return MIXER_ERROR_INVALID_STATE;
	}

	isReady = setReady;
	authorization = auth;
	versionId = versionId;
	shareCode = shareCode;

	state = chat_connecting;
	if (onStateChanged)
	{
		onStateChanged(callerContext, chat_disconnected, state);
		if (shutdownRequested)
		{
			return MIXER_ERROR_CANCELLED;
		}
	}

	// Create thread to open websocket and receive messages.
	incomingThread = std::thread(std::bind(&ChatHandler::run_incoming_thread, this));

	// Create thread to send messages over the open websocket.
	outgoingThread = std::thread(std::bind(&ChatHandler::run_outgoing_thread, this));

	return MIXER_OK;
}

state_change_event::state_change_event(chat_state currentState) : chat_event_internal(chat_event_type_state_change), currentState(currentState) {}