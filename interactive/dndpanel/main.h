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

#include "Professions/jobs.h"

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
        int Run(Chat::AuthPtr, DndConfigPtr);
        void ParticipantsChangedHandler(interactive_participant_action action, const interactive_participant* participant);
        void ErrorHandler(int errorCode, const char* errorMessage, size_t errorMessageLength);
        void InputHandler(const interactive_input* input);

        // IPanelLogicCallbacks
        int SendBraodcastUpdate(std::string& data) override;


    private:
        int SetupHandlers();

        PanelLogicPtr m_panelLogic;
        interactive_session m_session;

		Chat::AuthPtr m_auth;
		DndConfigPtr m_config;
    };

	DECLARE_SMARTPOINTER(ChatRunner);
	class ChatRunner :
		public SharedFromThis
	{
	public:
		int Run(Chat::AuthPtr, ChatConfigPtr,int);
		void ParticipantsChangedHandler(Chat::chat_participant_action action, const Chat::chat_participant* participant);
		
	private:
		int SetupHandlers();
		Chat::AuthPtr m_auth;
		ChatConfigPtr m_config;
		Chat::chat_session m_session;

	};	
}