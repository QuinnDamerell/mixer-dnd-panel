#pragma once
//nvs

#include <mutex>
#include <shared_mutex>
#include "rapidjson/document.h"

#include "internal/websocket.h"
#include "internal/interactive_session.h"
#include "internal/common.h"
#include "internal/json.h"
#include "rapidjson/document.h"

#include <queue>

#include "logger.h"

using namespace RAPIDJSON_NAMESPACE;

//

#include "common.h"
#include "config.h"

namespace ChatUtil
{


	#define RPC_EVENT_WELCOME_EVENT "WelcomeEvent"
	#define RPC_EVENT_CHAT_MESSAGE "ChatMessage"
	#define RPC_EVENT_REPLY "reply"
	#define RPC_EVENT_USER_JOIN "UserJoin"
	#define RPC_EVENT_USER_LEAVE "UserLeave"
	#define RPC_EVENT "event"

	#define DEFAULT_CONNECTION_RETRY_FREQUENCY_S 1
	#define MAX_CONNECTION_RETRY_FREQUENCY_S 8

	typedef void* chat_session;

	enum chat_participant_action
	{
		participant_join_c,
		participant_leave_c,
		participant_update_c
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

	enum chat_state
	{
		chat_disconnected,
		chat_connecting,
		chat_connected,
		chat_ready
	};

	struct chat_session_internal;


	// Interactive event types in priority order.
	enum chat_event_type
	{
		chat_event_type_error,
		chat_event_type_state_change,
		chat_event_type_http_response,
		chat_event_type_http_request,
		chat_event_type_rpc_reply,
		chat_event_type_rpc_method,
		chat_event_type_rpc_event,
	};

	struct chat_event_internal
	{
		const chat_event_type type;
		chat_event_internal(const chat_event_type type);
	};


	struct chat_object
	{
		const char* id;
		size_t idLength;
	};

	struct chat_control : public chat_object
	{
		const char* kind;
		size_t kindLength;
	};

	struct chat_control_pointer
	{
		const std::string sceneId;
		const std::string cachePointer;
		chat_control_pointer(std::string sceneId, std::string cachePointer) : sceneId(std::move(sceneId)), cachePointer(std::move(cachePointer)) {}
	};

	struct compare_event_priority
	{
		bool operator()(const std::shared_ptr<chat_event_internal> left, const std::shared_ptr<chat_event_internal> right)
		{
			return left->type > right->type;
		}
	};

	typedef std::map<std::string, std::string> scenes_by_id;
	typedef std::map<std::string, std::string> scenes_by_group;
	typedef std::map<std::string, chat_control_pointer> controls_by_id;
	typedef std::map<std::string, std::shared_ptr<rapidjson::Document>> participants_by_id;
	typedef std::function<int(chat_session_internal&, rapidjson::Document&)> method_handler_c;
	typedef std::map<std::string, method_handler_c> method_handlers_by_method;
	typedef std::map<unsigned int, std::pair<bool, method_handler_c>> reply_handlers_by_id;
	typedef std::function<int(const mixer_internal::http_response&)> http_response_handler;
	typedef std::pair<const mixer_result_code, const std::string> interactive_error;
	typedef std::priority_queue<std::shared_ptr<chat_event_internal>, std::vector<std::shared_ptr<chat_event_internal>>, compare_event_priority> chat_event_queue;


	typedef std::pair<const mixer_result_code, const std::string> chat_error;

	struct error_event : chat_event_internal
	{
		const chat_error error;
		error_event(const chat_error error) : chat_event_internal(chat_event_type_error), error(error) {}
	};

	enum chat_control_event
	{
		chat_control_created,
		chat_control_updated,
		chat_control_deleted
	};

	typedef void(*on_participants_changed_c)(void* context, chat_session session, chat_participant_action action, const chat_participant* participant);
	typedef void(*on_state_changed_c)(void* context, chat_session session, chat_state previousState, chat_state newState);
	typedef void(*on_control_changed_c)(void* context, chat_session session, chat_control_event eventType, const chat_control* control);

	DECLARE_SMARTPOINTER(Auth);
	class Auth :
		public SharedFromThis
	{
	public:
		int EnsureAuth(DnDPanel::DndConfigPtr);
		int EnsureAuth(DnDPanel::ChatConfigPtr);
		std::string authToken;
	};

	struct chat_session_internal
	{

		Document usersState;
		chat_session_internal();

		AuthPtr m_auth;

		// Configuration
		bool isReady;

		// State
		chat_state state;
		std::string authorization;
		std::string versionId;
		std::string shareCode;
		bool shutdownRequested;
		void* callerContext;
		std::atomic<uint32_t> packetId;
		int sequenceId;
		long long serverTimeOffsetMs;
		bool serverTimeOffsetCalculated;

		// Server time offset
		std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> getTimeSent;
		unsigned int getTimeRequestId;

		// Cached data
		std::shared_mutex scenesMutex;
		rapidjson::Document scenesRoot;
		bool scenesCached;
		scenes_by_id scenes;
		scenes_by_group scenesByGroup;
		bool groupsCached;
		controls_by_id controls;
		participants_by_id participants;

		// Event handlers
		on_input onInput;
		on_error onError;
		on_state_changed_c onStateChanged;
		on_participants_changed_c onParticipantsChanged;
		on_control_changed_c onControlChanged;
		on_transaction_complete onTransactionComplete;
		on_unhandled_method onUnhandledMethod;

		// Transactions that have been completed.
		std::map<std::string, interactive_error> completedTransactions;

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

		// Outgoing data
		void run_outgoing_thread();
		std::thread outgoingThread;
		std::mutex outgoingMutex;
		std::condition_variable outgoingCV;
		std::queue<std::shared_ptr<chat_event_internal>> outgoingEvents;
		void enqueue_outgoing_event(std::shared_ptr<chat_event_internal>&& ev);

		// Incoming data
		void run_incoming_thread();
		std::thread incomingThread;
		std::mutex incomingMutex;
		chat_event_queue incomingEvents;
		reply_handlers_by_id replyHandlersById;
		std::map<unsigned int, http_response_handler> httpResponseHandlers;
		void enqueue_incoming_event(std::shared_ptr<chat_event_internal>&& ev)
		{
			std::unique_lock<std::mutex> incomingLock(this->incomingMutex);
			this->incomingEvents.emplace(ev);
		}

		// Method handlers
		method_handlers_by_method methodHandlers;
	};


	int update_cached_control(chat_session_internal& session, chat_control& control, rapidjson::Value& controlJson);

	int update_control_pointers(chat_session_internal& session, const char* sceneId);

	typedef std::function<void(rapidjson::Document::AllocatorType& allocator, rapidjson::Value& value)> on_get_params;

	int create_method_json(chat_session_internal& session, const std::string& method, on_get_params getParams, bool discard, unsigned int* id, std::shared_ptr<rapidjson::Document>& methodDoc);

	struct rpc_method_event : chat_event_internal
	{
		const std::shared_ptr<rapidjson::Document> methodJson;
		rpc_method_event(std::shared_ptr<rapidjson::Document>&& methodJson) : chat_event_internal(chat_event_type_rpc_method), methodJson(methodJson) {}
	};

	struct rpc_event_event : chat_event_internal
	{
		const std::shared_ptr<rapidjson::Document> methodJson;
		rpc_event_event(std::shared_ptr<rapidjson::Document>&& methodJson) : chat_event_internal(chat_event_type_rpc_event), methodJson(methodJson) {}
	};

	// Queue a method to be sent out on the websocket. If handleImmediately is set to true, the handler will be called by the websocket receive thread rather than put on the reply queue.
	int queue_method(chat_session_internal& session, const std::string& method, on_get_params getParams, method_handler_c onReply, const bool handleImmediately = false);

	int send_ready_message(chat_session_internal& session, bool ready = true);

	int bootstrap(chat_session_internal& session);

	int cache_scenes(chat_session_internal& session);

	int cache_groups(chat_session_internal& session);

	int update_server_time_offset(chat_session_internal& session);

	int handle_hello(chat_session_internal& session, rapidjson::Document& doc);

	int handle_input(chat_session_internal& session, rapidjson::Document& doc);

	void parse_participant(rapidjson::Value& participantJson, chat_participant& participant);

	int handle_participants_change(chat_session_internal& session, rapidjson::Document& doc, chat_participant_action action);

	int handle_participants_join(chat_session_internal& session, rapidjson::Document& doc);

	int handle_participants_leave(chat_session_internal& session, rapidjson::Document& doc);

	int handle_participants_update(chat_session_internal& session, rapidjson::Document& doc);

	int handle_ready(chat_session_internal& session, rapidjson::Document& doc);

	int handle_group_changed(chat_session_internal& session, rapidjson::Document& doc);

	int handle_scene_changed(chat_session_internal& session, rapidjson::Document& doc);

	int handle_welcome_event(chat_session_internal& session, rapidjson::Document& doc);

	int cache_new_control(chat_session_internal& session, const char* sceneId, chat_control& control, rapidjson::Value& controlJson);

	int delete_cached_control(chat_session_internal& session, const char* sceneId, chat_control& control);

	void parse_control(rapidjson::Value& controlJson, chat_control& control);

	int handle_control_changed(chat_session_internal& session, rapidjson::Document& doc);

	void register_method_handlers(chat_session_internal& session);

	typedef void* chat_session;

	int chat_open_session(chat_session* sessionPtr);
	
	int get_interactive_hosts(chat_session_internal& session, std::vector<std::string>& interactiveHosts);

	struct rpc_reply_event : chat_event_internal
	{
		const unsigned int id;
		const std::shared_ptr<rapidjson::Document> replyJson;
		const method_handler_c replyHandler;
		rpc_reply_event(const unsigned int id, std::shared_ptr<rapidjson::Document>&& replyJson, const method_handler_c handler) : chat_event_internal(chat_event_type_rpc_reply), id(id), replyJson(replyJson), replyHandler(replyHandler) {}
	};
	
	struct state_change_event : chat_event_internal
	{
		const chat_state currentState;
		state_change_event(chat_state currentState);
	};
	
	typedef std::map<std::string, std::string> http_headers;


	struct http_request_event : chat_event_internal
	{
		const unsigned int packetId;
		const std::string uri;
		const std::string verb;
		const std::map<std::string, std::string> headers;
		const std::string body;
		http_request_event(const unsigned int packetId, const std::string& uri, const std::string& verb, const http_headers* headers, const std::string* body) :
			chat_event_internal(chat_event_type_http_request), packetId(packetId), uri(uri), verb(verb), headers(nullptr == headers ? http_headers() : *headers), body(nullptr == body ? std::string() : *body)
		{
		}
	};

	struct http_response_event : chat_event_internal
	{
		const mixer_internal::http_response response;
		const http_response_handler responseHandler;
		http_response_event(mixer_internal::http_response&& response, const http_response_handler handler) : chat_event_internal(chat_event_type_http_response), response(response), responseHandler(handler) {}
	};

	int chat_connect(chat_session session, const char* auth, const char* versionId, const char* shareCode, bool setReady);

	int route_method(chat_session_internal& session, rapidjson::Document& doc);

	int route_event(chat_session_internal& session, rapidjson::Document& doc);

	int chat_run(chat_session session, unsigned int maxEventsToProcess);

	void chat_close_session(chat_session session);
}