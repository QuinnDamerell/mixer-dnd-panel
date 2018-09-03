#include "main.h"

#include "rapidjson/document.h"

#include <windows.h>
#include <shellapi.h>

#include <iostream>
#include <thread>

#define MIXER_DEBUG 1

using namespace DnDPanel;

int main()
{
#if MIXER_DEBUG
	interactive_config_debug(interactive_debug_trace, [](const interactive_debug_level dbgMsgType, const char* dbgMsg, size_t dbgMsgSize)
	{
		std::cout << dbgMsg << std::endl;
	});
#endif

    DndRunnerPtr runner = std::make_shared<DndRunner>();
    return runner->Run();
}

int DndRunner::Run()
{
    // Setup the config
    m_config = std::make_shared<DndConfig>();
    int err = 0;
    if ((err = m_config->Init()))
    {
        Logger::Error("Failed to read config.");
        return err;
    }

    // Create our logic class.
    m_panelLogic = std::make_shared<PanelLogic>(GetSharedPtr<DndRunner>());

    // Check auth
    if((err = EnsureAuth()))
    {
        Logger::Error("Failed setup auth.");
        return err;
    }

    // Setup interactive
    if ((err = interactive_open_session(&m_session)))
    {
        Logger::Error("Failed to setup interactive!");
        return err;
    }

    // Setup handlers
    if ((err = SetupHandlers()))
    {
        Logger::Error("Failed to setup handlers");
        return err;
    }

    // Connect
    if ((err = interactive_connect(m_session, m_authToken.c_str(), m_config->InteractiveId.c_str(), m_config->ShareCode.c_str(), true)))
    {
        Logger::Error("Failed to connect to interactive!");
        return err;
    }

    // Run! (like this was a game loop)
    high_clock::time_point lastTickRun = high_clock::now();
    for (;;)
    {
        // Run interactive
        if ((err = interactive_run(m_session, 500)))
        {
            break;
        }

        // Run the update logic.
        high_clock::time_point now = high_clock::now();
        m_panelLogic->ExectueTick(now - lastTickRun);
        lastTickRun = now;

        // Sleepy time.
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    interactive_close_session(m_session);
    return err;
}

int DndRunner::SendBraodcastUpdate(std::string& data)
{
    std::string fullJson = "{\"scope\":[\"group:default\"], \"data\":" + data + "}";
    return interactive_queue_method(m_session, "broadcastEvent", fullJson.c_str(), nullptr);
}

int DndRunner::EnsureAuth()
{
    int err = 0;

    // Try to use the old token if it exists.
    do
    {
        // If we have a token, try to refresh.
        if (m_config->RefreshToken.length() > 0)
        {
            char newToken[1024];
            size_t newTokenLength = _countof(newToken);
            err = interactive_auth_refresh_token(m_config->ClientId.c_str(), nullptr, m_config->RefreshToken.c_str(), newToken, &newTokenLength);
            if (!err)
            {
                m_config->RefreshToken = std::string(newToken, newTokenLength);
                break;
            }
            else
            {
                // If failed clear the token
                m_config->RefreshToken = "";
            }
        }
        else
        {
            // Start a clean auth
            char shortCode[7];
            size_t shortCodeLength = _countof(shortCode);
            char shortCodeHandle[1024];
            size_t shortCodeHandleLength = _countof(shortCodeHandle);
            err = interactive_auth_get_short_code(CLIENT_ID, nullptr, shortCode, &shortCodeLength, shortCodeHandle, &shortCodeHandleLength);
            if (err) return err;

            // Pop the browser for the user to approve access.
            std::string authUrl = std::string("https://www.mixer.com/go?code=") + shortCode;
            ShellExecuteA(0, 0, authUrl.c_str(), nullptr, nullptr, SW_SHOW);

            // Wait for OAuth token response.
            char refreshTokenBuffer[1024];
            size_t refreshTokenLength = _countof(refreshTokenBuffer);
            err = interactive_auth_wait_short_code(CLIENT_ID, nullptr, shortCodeHandle, refreshTokenBuffer, &refreshTokenLength);
            if (err)
            {
                if (MIXER_ERROR_TIMED_OUT == err)
                {
                    std::cout << "Authorization timed out, user did not approve access within the time limit." << std::endl;
                }
                else if (MIXER_ERROR_AUTH_DENIED == err)
                {
                    std::cout << "User denied access." << std::endl;
                }

                return err;
            }

            // Cache the refresh token
            m_config->RefreshToken = std::string(refreshTokenBuffer, refreshTokenLength);
            break;
        }
    } while (m_config->RefreshToken.length() == 0);

    // Write the config.
    m_config->Write();

    // Extract the authorization header from the refresh token.
    char authBuffer[1024];
    size_t authBufferLength = _countof(authBuffer);
    err = interactive_auth_parse_refresh_token(m_config->RefreshToken.c_str(), authBuffer, &authBufferLength);
    if (err)
    {
        return err;
    }

    // Success!
    m_authToken = std::string(authBuffer, authBufferLength);
    return 0;
}

void DndRunner::ParticipantsChangedHandler(interactive_participant_action action, const interactive_participant* participant)
{
    std::stringstream ss;
    ss << "User [" << participant->userId << "] " << std::string(participant->userName, participant->usernameLength) << (action == interactive_participant_action::participant_join ? " joined " : " left ") << "\n";
    Logger::Info(ss.str());
}

void DndRunner::ErrorHandler(int errorCode, const char* errorMessage, size_t errorMessageLength)
{
    std::stringstream ss;
    ss << "Interactive Error Reported! " << errorCode << std::string(errorMessage, errorMessageLength) << "\n";
    Logger::Error(ss.str());
}

void DndRunner::InputHandler(const interactive_input* input)
{
    // Send the input to the logic handler
    m_panelLogic->IncomingInput(input->jsonData, input->jsonDataLength);
}

void participants_changed_handler(void* context, interactive_session session, interactive_participant_action action, const interactive_participant* participant)
{
    if (context)
    {
        DndRunner* runner = (DndRunner*)context;
        runner->ParticipantsChangedHandler(action, participant);
    }
}

void handle_error(void* context, interactive_session session, int errorCode, const char* errorMessage, size_t errorMessageLength)
{
    if (context)
    {
        DndRunner* runner = (DndRunner*)context;
        runner->ErrorHandler(errorCode, errorMessage, errorMessageLength);
    }
}

void input_handler(void* context, interactive_session session, const interactive_input* input)
{
    if (context)
    {
        DndRunner* runner = (DndRunner*)context;
        runner->InputHandler(input);
    }
}

int DndRunner::SetupHandlers()
{
    int err = 0;

    // Set this object as the context.
    err = interactive_set_session_context(m_session, this);
    if (err) return err;

    // Register a callback for errors.
    err = interactive_set_error_handler(m_session, handle_error);
    if (err) return err;

    err = interactive_set_participants_changed_handler(m_session, participants_changed_handler);
    if (err) return err;

    // Register a callback for button presses.
    err = interactive_set_input_handler(m_session, input_handler);
    if (err) return err;

    return err;
}