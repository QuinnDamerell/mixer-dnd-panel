#pragma once

#include "common.h"
#include "config.h"
#include "panellogic.h"

#include "interactivity.h"

//nvs
#include "chatutil.h"

#include <mutex>
#include <shared_mutex>
#include "rapidjson/document.h"

#include "internal/websocket.h"
#include "internal/interactive_session.h"
#include "internal/common.h"
#include "internal/json.h"
#include "rapidjson/document.h"

#include <queue>

using namespace RAPIDJSON_NAMESPACE;

//
namespace DnDPanel
{
	DECLARE_SMARTPOINTER(DndRunner);
    class DndRunner :
        public SharedFromThis,
        public IPanelLogicCallbacks
    {
    public:
        int Run(ChatUtil::AuthPtr, DndConfigPtr);
        void ParticipantsChangedHandler(interactive_participant_action action, const interactive_participant* participant);
        void ErrorHandler(int errorCode, const char* errorMessage, size_t errorMessageLength);
        void InputHandler(const interactive_input* input);

        // IPanelLogicCallbacks
        int SendBraodcastUpdate(std::string& data) override;


    private:
        int SetupHandlers();

        PanelLogicPtr m_panelLogic;
        interactive_session m_session;

		ChatUtil::AuthPtr m_auth;
		DndConfigPtr m_config;
    };

	DECLARE_SMARTPOINTER(ChatRunner);
	class ChatRunner :
		public SharedFromThis
	{
	public:
		int Run(ChatUtil::AuthPtr, ChatConfigPtr);
		void ParticipantsChangedHandler(ChatUtil::chat_participant_action action, const ChatUtil::chat_participant* participant);
		
	private:
		int SetupHandlers();
		ChatUtil::AuthPtr m_auth;
		ChatConfigPtr m_config;
		ChatUtil::chat_session m_session;

	};	
}