#pragma once

#include <atomic>
#include <shared_mutex>

#include "rapidjson/document.h"

#include "Chat/auth.h"
#include "Chat/chat_state.h"
#include "Chat/chat_control_pointer.h"
#include "Chat/chat_participant_action.h"
#include "Chat/chat_control_event.h"
#include "Chat/chat_participant.h"
#include "Chat/chat_object.h"
#include "Chat/chat_event_internal.h"

#include "interactivity.h"

#include "internal/websocket.h"
#include "internal/interactive_session.h"

#include "Professions/jobs.h"
#include "Classes/classes.h"

namespace ChatSession
{
	struct chat_session_internal;

	typedef void* chat_session;
	typedef std::map<std::string, std::string> scenes_by_id;
	typedef std::map<std::string, std::string> scenes_by_group;
	typedef std::map<std::string, Chat::chat_control_pointer> controls_by_id;
	typedef std::map<std::string, std::shared_ptr<rapidjson::Document>> participants_by_id;
	typedef std::pair<const mixer_result_code, const std::string> interactive_error;
	typedef std::function<int(chat_session_internal&, rapidjson::Document&)> method_handler_c;
	typedef std::map<std::string, method_handler_c> method_handlers_by_method;
	typedef std::map<unsigned int, std::pair<bool, method_handler_c>> reply_handlers_by_id;
	typedef std::function<int(const mixer_internal::http_response&)> http_response_handler;

	struct compare_event_priority
	{
		bool operator()(const std::shared_ptr<Chat::chat_event_internal> left, const std::shared_ptr<Chat::chat_event_internal> right)
		{
			return left->type > right->type;
		}
	};
	typedef std::priority_queue<std::shared_ptr<Chat::chat_event_internal>, std::vector<std::shared_ptr<Chat::chat_event_internal>>, compare_event_priority> chat_event_queue;

	typedef void(*on_state_changed_c)(void* context, chat_session session, Chat::chat_state previousState, Chat::chat_state newState);
	typedef void(*on_participants_changed_c)(void* context, chat_session session, Chat::chat_participant_action action, const Chat::chat_participant* participant);
	typedef void(*on_control_changed_c)(void* context, chat_session session, Chat::chat_control_event eventType, const Chat::chat_control* control);

	struct chat_session_internal
	{
		int chatToConnect;
		rapidjson::Document usersState;
		chat_session_internal();

		// Helper lists
		Professions::jobslistPtr jobList;
		Classes::classInfoListPtr classList;

		Chat::AuthPtr m_auth;

		// Configuration
		bool isReady;

		// State
		Chat::chat_state state;
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

		// Method handlers
		method_handlers_by_method methodHandlers;
		std::vector<std::pair<std::string, int>> levels;
	};
}