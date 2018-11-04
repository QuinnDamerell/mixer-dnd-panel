#pragma once

#include <atomic>
#include <shared_mutex>

#include "rapidjson/document.h"

#include "../Chat/auth.h"
#include "../Chat/chat_state.h"
#include "../Chat/chat_control_pointer.h"
#include "../Chat/chat_participant_action.h"
#include "../Chat/chat_control_event.h"
#include "../Chat/chat_participant.h"
#include "../Chat/chat_object.h"
#include "../Chat/chat_event_internal.h"

#include "interactivity.h"

#include "internal/websocket.h"
#include "internal/interactive_session.h"

#include "../Professions/jobs.h"
#include "../Classes/classes.h"
#include "../Quests/QuestManager.h"

namespace ChatSession
{
	struct chat_session_internal;

	typedef void* chat_session;
	typedef std::map<std::string, std::string> scenes_by_id;
	typedef std::map<std::string, std::string> scenes_by_group;
	typedef std::map<std::string, Chat::chat_control_pointer> controls_by_id;
	typedef std::map<std::string, std::shared_ptr<rapidjson::Document>> participants_by_id;
	typedef std::pair<const mixer_result_code, const std::string> interactive_error;
	
	

	struct compare_event_priority
	{
		bool operator()(const std::shared_ptr<Chat::chat_event_internal> left, const std::shared_ptr<Chat::chat_event_internal> right)
		{
			return left->type > right->type;
		}
	};
	typedef std::priority_queue<std::shared_ptr<Chat::chat_event_internal>, std::vector<std::shared_ptr<Chat::chat_event_internal>>, compare_event_priority> chat_event_queue;

	typedef void(*on_control_changed_c)(void* context, chat_session session, Chat::chat_control_event eventType, const Chat::chat_control* control);

	struct chat_session_internal
	{		
		chat_session_internal();

		// Configuration
		bool isReady;

		// State
		std::string versionId;
		std::string shareCode;
		long long serverTimeOffsetMs;

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
		on_control_changed_c onControlChanged;
		on_transaction_complete onTransactionComplete;
		on_unhandled_method onUnhandledMethod;

		// Transactions that have been completed.
		std::map<std::string, interactive_error> completedTransactions;

	};
}