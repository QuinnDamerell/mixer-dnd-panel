#pragma once

#include "common.h"
#include "config.h"
#include "panellogic.h"

#include "interactivity.h"

namespace DnDPanel
{
    DECLARE_SMARTPOINTER(DndRunner);
    class DndRunner :
        public SharedFromThis,
        public IPanelLogicCallbacks
    {
    public:
        int Run();
        void ParticipantsChangedHandler(interactive_participant_action action, const interactive_participant* participant);
        void ErrorHandler(int errorCode, const char* errorMessage, size_t errorMessageLength);
        void InputHandler(const interactive_input* input);

        // IPanelLogicCallbacks
        int SendBraodcastUpdate(std::string& data) override;

    private:
        int EnsureAuth();
        int SetupHandlers();

        PanelLogicPtr m_panelLogic;
        interactive_session m_session;
        DndConfigPtr m_config;
        std::string m_authToken;
    };
}