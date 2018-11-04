#pragma once

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

#include "../Chat/chat_event_internal.h"


namespace Chat
{

	#define DEFAULT_CONNECTION_RETRY_FREQUENCY_S 1
	#define MAX_CONNECTION_RETRY_FREQUENCY_S 8
	
	typedef std::function<void(rapidjson::Document::AllocatorType& allocator, rapidjson::Value& value)> on_get_params;

	int create_method_json(Chat::ChatHandlerPtr chatHandler, const std::string& method, on_get_params getParams, bool discard, unsigned int* id, std::shared_ptr<rapidjson::Document>& methodDoc);
	
	struct rpc_event_event : chat_event_internal
	{
		const std::shared_ptr<rapidjson::Document> methodJson;
		rpc_event_event(std::shared_ptr<rapidjson::Document>&& methodJson) : chat_event_internal(chat_event_type_rpc_event), methodJson(methodJson) {}
	};

	// Queue a method to be sent out on the websocket. If handleImmediately is set to true, the handler will be called by the websocket receive thread rather than put on the reply queue.
	int queue_method(Chat::ChatHandlerPtr chatHandler, const std::string& method, on_get_params getParams, Chat::method_handler_c onReply, const bool handleImmediately = false);

	int send_ready_message(Chat::ChatHandlerPtr chatHandler, bool ready = true);

	int handle_hello(ChatBot::BotPtr bot, rapidjson::Document& doc);

	void parse_participant(rapidjson::Value& participantJson, chat_participant& participant);
	
	int cache_new_control(ChatSession::chat_session_internal& session, const char* sceneId, chat_control& control, rapidjson::Value& controlJson);

	int delete_cached_control(ChatSession::chat_session_internal& session, const char* sceneId, chat_control& control);

	void parse_control(rapidjson::Value& controlJson, chat_control& control);

	typedef void* chat_session;	
	
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
	
	void chat_close_session(ChatBot::BotPtr chatBot);
}