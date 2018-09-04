#include "panellogic.h"

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include <algorithm>

using namespace DnDPanel;
using namespace rapidjson;

#define PLAYER_COUNT 6
#define PANEL_UPDATE_TIME_MS 500

PanelLogic::PanelLogic(IPanelLogicCallbacksWeakPtr callback) :
    m_callback(callback),
    m_lastUpdateSend(high_clock::now())
{
    m_players.resize(PLAYER_COUNT);
    for(int i = 0; i < PLAYER_COUNT; i++)
    {
        m_players[i] = 0;
    }
};

int PanelLogic::ExectueTick(high_clock::duration timeElipased)
{
    size_t msSinceLastSend = std::chrono::duration_cast<ms>(high_clock::now() - m_lastUpdateSend).count();
    if (msSinceLastSend < PANEL_UPDATE_TIME_MS)
    {
        return 0;
    }
    m_lastUpdateSend = high_clock::now();

    // Update the values
    for (int i = 0; i < m_players.size(); i++)
    {
        // Balance toward 0
        if (m_players[i] != 0)
        {
            int newValue = (int)std::pow((double)std::abs(m_players[i]), 0.98);
            m_players[i] = m_players[i] > 0 ? newValue : -newValue;
        }
    }

    // Build the json
    Document d;
    d.SetObject();
    {
        Value a;
        a.SetArray();
        for(int& i : m_players)
        {
            a.PushBack(i, d.GetAllocator());
        }
        d.AddMember("player_values", a, d.GetAllocator());
    }

    // Get the string.
    rapidjson::StringBuffer buffer;
    buffer.Clear();
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    d.Accept(writer);
    std::string json = buffer.GetString();

    // Send the update.
    if (auto callback = m_callback.lock())
    {
        callback->SendBraodcastUpdate(json);
    }
    return 0;
}

void PanelLogic::HandlePlayerInflunce(int playerIndex, bool isAdd)
{
    if (playerIndex >= m_players.size())
    {
        return;
    }
    m_players[playerIndex] += (isAdd ? 30 : -30);
    m_players[playerIndex] = std::min(std::max(m_players[playerIndex], -200), 200);
}

#define COMMAND_ACTION_TOKEN "action"
#define COMMAND_ACTION_PLAYER_INFLUENCE "player-influence"

int PanelLogic::IncomingInput(const char* json, const size_t jsonLen)
{
    // Parse the json
    Document d;
    try
    {
        d.Parse(json, jsonLen);
    }
    catch (std::exception& e)
    {
        Logger::Error("Failed to parse incoming input json");
        return 1;
    }

    // Get the input object
    if (!d.IsObject()) return 1;
    if (!d.HasMember("input") || !d["input"].IsObject()) return 1;
    Value inputObj = d["input"].GetObject();

    // Get the action
    if (!inputObj.HasMember(COMMAND_ACTION_TOKEN) || !inputObj[COMMAND_ACTION_TOKEN].IsString()) return 1;
    std::string action = inputObj[COMMAND_ACTION_TOKEN].GetString();

    if (strcmp(action.c_str(), COMMAND_ACTION_PLAYER_INFLUENCE) == 0)
    {
        if (!inputObj.HasMember("player") || !inputObj.HasMember("isAdd") || !inputObj["player"].IsInt() || !inputObj["isAdd"].IsBool())
        {
            return 1;
        }
        HandlePlayerInflunce(inputObj["player"].GetInt(), inputObj["isAdd"].GetBool());
    }
    return 0;
}
