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

#include "common.h"
#include "config.h"

#include "chatbot.h"

#include "chat_session_internal.h"

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

	struct compare_event_priority
	{
		bool operator()(const std::shared_ptr<chat_event_internal> left, const std::shared_ptr<chat_event_internal> right)
		{
			return left->type > right->type;
		}
	};	
		
	typedef std::priority_queue<std::shared_ptr<chat_event_internal>, std::vector<std::shared_ptr<chat_event_internal>>, compare_event_priority> chat_event_queue;


	typedef std::pair<const mixer_result_code, const std::string> chat_error;

	struct error_event : chat_event_internal
	{
		const chat_error error;
		error_event(const chat_error error) : chat_event_internal(chat_event_type_error), error(error) {}
	};	

	int update_cached_control(ChatSession::chat_session_internal& session, chat_control& control, rapidjson::Value& controlJson);

	int update_control_pointers(ChatSession::chat_session_internal& session, const char* sceneId);

	typedef std::function<void(rapidjson::Document::AllocatorType& allocator, rapidjson::Value& value)> on_get_params;

	int create_method_json(ChatSession::chat_session_internal& session, const std::string& method, on_get_params getParams, bool discard, unsigned int* id, std::shared_ptr<rapidjson::Document>& methodDoc);

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
	int queue_method(ChatSession::chat_session_internal& session, const std::string& method, on_get_params getParams, ChatSession::method_handler_c onReply, const bool handleImmediately = false);

	int send_ready_message(ChatSession::chat_session_internal& session, bool ready = true);

	int bootstrap(ChatSession::chat_session_internal& session);

	int cache_scenes(ChatSession::chat_session_internal& session);

	int cache_groups(ChatSession::chat_session_internal& session);

	int update_server_time_offset(ChatSession::chat_session_internal& session);

	int handle_hello(ChatSession::chat_session_internal& session, rapidjson::Document& doc);

	int handle_input(ChatSession::chat_session_internal& session, rapidjson::Document& doc);

	void parse_participant(rapidjson::Value& participantJson, chat_participant& participant);

	int handle_participants_change(ChatSession::chat_session_internal& session, rapidjson::Document& doc, ChatUtil::chat_participant_action action);

	int handle_participants_join(ChatSession::chat_session_internal& session, rapidjson::Document& doc);

	int handle_participants_leave(ChatSession::chat_session_internal& session, rapidjson::Document& doc);

	int handle_participants_update(ChatSession::chat_session_internal& session, rapidjson::Document& doc);

	int handle_ready(ChatSession::chat_session_internal& session, rapidjson::Document& doc);

	int handle_group_changed(ChatSession::chat_session_internal& session, rapidjson::Document& doc);

	int handle_scene_changed(ChatSession::chat_session_internal& session, rapidjson::Document& doc);

	int handle_welcome_event(ChatSession::chat_session_internal& session, rapidjson::Document& doc);

	int cache_new_control(ChatSession::chat_session_internal& session, const char* sceneId, chat_control& control, rapidjson::Value& controlJson);

	int delete_cached_control(ChatSession::chat_session_internal& session, const char* sceneId, chat_control& control);

	void parse_control(rapidjson::Value& controlJson, chat_control& control);

	int handle_control_changed(ChatSession::chat_session_internal& session, rapidjson::Document& doc);

	void register_method_handlers(ChatSession::chat_session_internal& , ChatBot::BotPtr);

	typedef void* chat_session;

	int chat_open_session(chat_session*, ChatBot::BotPtr);
	
	int get_interactive_hosts(ChatSession::chat_session_internal& session, std::vector<std::string>& interactiveHosts);

	struct rpc_reply_event : chat_event_internal
	{
		const unsigned int id;
		const std::shared_ptr<rapidjson::Document> replyJson;
		const ChatSession::method_handler_c replyHandler;
		rpc_reply_event(const unsigned int id, std::shared_ptr<rapidjson::Document>&& replyJson, const ChatSession::method_handler_c handler) : chat_event_internal(chat_event_type_rpc_reply), id(id), replyJson(replyJson), replyHandler(replyHandler) {}
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
		const ChatSession::http_response_handler responseHandler;
		http_response_event(mixer_internal::http_response&& response, const ChatSession::http_response_handler handler) : chat_event_internal(chat_event_type_http_response), response(response), responseHandler(handler) {}
	};

	int chat_connect(chat_session session, const char* auth, const char* versionId, const char* shareCode, bool setReady);

	int route_method(ChatSession::chat_session_internal& session, rapidjson::Document& doc);

	int route_event(ChatSession::chat_session_internal& session, rapidjson::Document& doc);

	int chat_run(chat_session session, unsigned int maxEventsToProcess);

	void chat_close_session(chat_session session);
}