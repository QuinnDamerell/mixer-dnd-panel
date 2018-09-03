#pragma once

#include "common.h"
#include "config.h"

#include <vector>

namespace DnDPanel
{
    DECLARE_SMARTPOINTER(IPanelLogicCallbacks);
    class IPanelLogicCallbacks
    {
    public:
        virtual int SendBraodcastUpdate(std::string& data) = 0;
    };

    DECLARE_SMARTPOINTER(PanelLogic);
    class PanelLogic
    {
    public:
        PanelLogic(IPanelLogicCallbacksWeakPtr callback);

        int ExectueTick(high_clock::duration timeElipased);
        int IncomingInput(const char* json, const size_t jsonLen);

    private:
        void HandlePlayerInflunce(int playerNumber, bool add);

        IPanelLogicCallbacksWeakPtr m_callback;
        high_clock::time_point m_lastUpdateSend;
        std::vector<int> m_players;
    };
}