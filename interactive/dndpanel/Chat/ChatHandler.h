#pragma once


#include <map>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>

#include "rapidjson/document.h"

#include "../dndpanel/common.h"
#include "../dndpanel/config.h"

#include "interactivity.h"
#include "internal/interactive_event.h"
#include "chat_state.h"
#include "internal/websocket.h"

//#include "chat_participant_action.h"

#include "chat_event_internal.h"



#include "internal/interactive_session.h"


namespace Chat
{

	#define RPC_EVENT_WELCOME_EVENT "WelcomeEvent"
	#define RPC_EVENT_CHAT_MESSAGE "ChatMessage"
	#define RPC_EVENT_REPLY "reply"
	#define RPC_EVENT_USER_JOIN "UserJoin"
	#define RPC_EVENT_USER_LEAVE "UserLeave"
	#define RPC_EVENT "event"

	enum chat_participant_action
	{
		participant_join_c,
		participant_leave_c,
		participant_update_c
	};

	DECLARE_SMARTPOINTER(Auth);
	class Auth :
		public SharedFromThis
	{
	public:
		int EnsureAuth(DnDPanel::DndConfigPtr);
		int EnsureAuth(DnDPanel::ChatConfigPtr);
		std::string getAuthToken();

	private:
		int chat_auth_get_short_code(const char* clientId, const char* clientSecret, char* shortCode, size_t* shortCodeLength, char* shortCodeHandle, size_t* shortCodeHandleLength);
		std::string authToken;
	};

	struct chat_participant : public interactive_object
	{
		unsigned int userId;
		const char* userName;
		size_t usernameLength;
		unsigned int level;
		unsigned long long lastInputAtMs;
		unsigned long long connectedAtMs;
		bool disabled;
		const char* groupId;
		size_t groupIdLength;
	};

	DECLARE_SMARTPOINTER(ChatHandler);

	typedef std::pair<const mixer_result_code, const std::string> interactive_error;
	typedef std::function<int(ChatHandlerPtr chatHandler, rapidjson::Document&)> method_handler_c;
	typedef std::map<unsigned int, std::pair<bool, method_handler_c>> reply_handlers_by_id;
	typedef std::function<int(const mixer_internal::http_response&)> http_response_handler;
	typedef void(*on_state_changed_c)(void* context, Chat::chat_state previousState, Chat::chat_state newState);
	typedef std::map<std::string, method_handler_c> method_handlers_by_method_c;
	typedef void(*on_chat_participants_changed_c)(void* context, Chat::chat_participant_action action, const chat_participant* participant);
	typedef std::function<int(const mixer_internal::http_response&)> http_response_handler;

	struct rpc_method_event : chat_event_internal
	{
		const std::shared_ptr<rapidjson::Document> methodJson;
		rpc_method_event(std::shared_ptr<rapidjson::Document>&& methodJson) : chat_event_internal(chat_event_type_rpc_method), methodJson(methodJson) {}
	};

	struct http_response_event : chat_event_internal
	{
		const mixer_internal::http_response response;
		const http_response_handler responseHandler;
		http_response_event(mixer_internal::http_response&& response, const http_response_handler handler) : chat_event_internal(chat_event_type_http_response), response(response), responseHandler(handler) {}
	};

	struct rpc_reply_event : chat_event_internal
	{
		const unsigned int id;
		const std::shared_ptr<rapidjson::Document> replyJson;
		const method_handler_c replyHandler;
		rpc_reply_event(const unsigned int id, std::shared_ptr<rapidjson::Document>&& replyJson, const method_handler_c handler) : chat_event_internal(chat_event_type_rpc_reply), id(id), replyJson(replyJson), replyHandler(replyHandler) {}
	};

	struct state_change_event : Chat::chat_event_internal
	{
		const Chat::chat_state currentState;
		state_change_event(Chat::chat_state currentState);
	};

	/// <summary>
	/// Callback when any method is called that is not handled by the existing callbacks. This may be useful for more advanced scenarios or future protocol changes that may not have existed in this version of the library.
	/// </summary>
	typedef void(*on_chat_unhandled_method)(void* context, const char* methodJson, size_t methodJsonLength);

	/// <summary>
	/// Callback for any errors that may occur during the session.
	/// </summary>
	/// <remarks>
	/// This is the only callback function that may be called from a thread other than the thread that calls <c>interactive_run</c>.
	/// </remarks>
	typedef void(*on_chat_error)(void* context, int errorCode, const char* errorMessage, size_t errorMessageLength);

	struct compare_event_priority
	{
		bool operator()(const std::shared_ptr<Chat::chat_event_internal> left, const std::shared_ptr<Chat::chat_event_internal> right)
		{
			return left->type > right->type;
		}
	};
	typedef std::priority_queue<std::shared_ptr<Chat::chat_event_internal>, std::vector<std::shared_ptr<Chat::chat_event_internal>>, compare_event_priority> chat_event_queue;

	struct user_object
	{
		int userid;
		int channelid;
	};

	typedef std::pair<const mixer_result_code, const std::string> chat_error;

	struct error_event : chat_event_internal
	{
		const chat_error error;
		error_event(const chat_error error) : chat_event_internal(chat_event_type_error), error(error) {}
	};

	
	class ChatHandler :
		public SharedFromThis
	{
	public:
		ChatHandler();

		int handle_chat_message(rapidjson::Document& doc);

		int handle_reply(rapidjson::Document& doc);

		int handle_user_join(rapidjson::Document& doc);

		int handle_user_leave(rapidjson::Document& doc);

		void sendMessage(std::string message);

		void sendWhisper(std::string message, std::string target);

		void deleteMessage(std::string id);

		int chat_connect(const char* auth, const char* versionId, const char* shareCode, bool setReady);

		user_object getUserInfo()
		{
			user_object result;
			result.userid = -1;
			result.channelid = -1;

			mixer_internal::http_response response;
			static std::string hostsUri = "https://mixer.com/api/v1/users/current";
			mixer_internal::http_headers headers;
			//headers.emplace("Content-Type", "application/json");
			headers.emplace("Authorization", m_auth->getAuthToken());

			// Critical Section: Http request.
			{
				std::unique_lock<std::mutex> httpLock(httpMutex);
				http->make_request(hostsUri, "GET", &headers, "", response);
			}

			if (200 != response.statusCode)
			{
				std::string errorMessage = "Failed to acquire chat access.";
				DnDPanel::Logger::Error(std::to_string(response.statusCode) + " " + errorMessage);
				enqueue_incoming_event(std::make_shared<error_event>(chat_error(MIXER_ERROR_NO_HOST, std::move(errorMessage))));

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

		void register_method_handlers();

		int chat_open_session()
		{
			// Register method handlers
			register_method_handlers();

			// Initialize Http and Websocket clients
			http = mixer_internal::http_factory::make_http_client();
			ws = mixer_internal::websocket_factory::make_websocket();

			return MIXER_OK;
		}

		int message_id = 1000;

		std::map<std::string, std::function<void(rapidjson::Document&)>> funcMap;

		bool shutdownRequested;
		
		// Outgoing data
		void run_outgoing_thread();
		std::thread outgoingThread;
		std::mutex outgoingMutex;
		std::condition_variable outgoingCV;
		std::queue<std::shared_ptr<Chat::chat_event_internal>> outgoingEvents;
		void enqueue_outgoing_event(std::shared_ptr<Chat::chat_event_internal>&& ev)
		{
			std::unique_lock<std::mutex> outgoingLock(this->outgoingMutex);
			this->outgoingEvents.emplace(ev);
		}

		// Incoming data
		void run_incoming_thread();
		std::thread incomingThread;
		std::mutex incomingMutex;
		chat_event_queue incomingEvents;
		reply_handlers_by_id replyHandlersById;
		std::map<unsigned int, http_response_handler> httpResponseHandlers;
		void enqueue_incoming_event(std::shared_ptr<Chat::chat_event_internal>&& ev)
		{
			std::unique_lock<std::mutex> incomingLock(this->incomingMutex);
			this->incomingEvents.emplace(ev);
		}

		// Http
		std::unique_ptr<mixer_internal::http_client> http;
		std::mutex httpMutex;

		// Websocket
		std::mutex websocketMutex;
		std::unique_ptr<mixer_internal::websocket> ws;
		bool wsOpen;

		// Websocket handlers
		void handle_ws_open(const mixer_internal::websocket& socket, const std::string& message);
		void handle_ws_message(const mixer_internal::websocket& socket, const std::string& message);
		void handle_ws_close(const mixer_internal::websocket& socket, unsigned short code, const std::string& message);

		// State
		Chat::chat_state state;
		std::string authorization;
		bool serverTimeOffsetCalculated;
		void* callerContext;
		std::atomic<uint32_t> packetId;
		int sequenceId;

		// Cached data
		bool scenesCached;
		bool groupsCached;

		// Configuration
		bool isReady;

		// Event Handler
		on_state_changed_c onStateChanged;
		on_chat_unhandled_method onUnhandledMethod;
		on_chat_error onError;
		on_chat_participants_changed_c onParticipantsChanged;
		on_input onInput;

		// Method Handlers
		method_handlers_by_method_c methodHandlers;

		// Auth
		Chat::AuthPtr m_auth;

		// Chat Info
		int chatToConnect;
	};
}