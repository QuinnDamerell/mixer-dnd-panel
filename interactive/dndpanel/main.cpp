#include "main.h"

#include <windows.h>
#include <shellapi.h>

#include <iostream>
#include <thread>

#include <stdint.h>

#define MIXER_DEBUG 0

#undef GetObject

#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
#include <rapidjson/writer.h>

#include "chatutil.h"
#include "chatbot.h"

#include <iostream>
#include <iomanip>
#include <fstream>

using namespace DnDPanel;
using namespace ChatUtil;
using namespace ChatBot;

void runChat(ChatRunnerPtr chatRunner, AuthPtr auth, ChatConfigPtr config, int channelToConnectTo)
{
	chatRunner->Run(auth, config, channelToConnectTo);
}

void runPanel(DndRunnerPtr interactiveRunner, AuthPtr auth, DndConfigPtr config)
{
	interactiveRunner->Run(auth, config);
}


int main()
{
#if MIXER_DEBUG
	interactive_config_debug(interactive_debug_trace, [](const interactive_debug_level dbgMsgType, const char* dbgMsg, size_t dbgMsgSize)
	{
		std::cout << dbgMsg << std::endl;
	});
#endif

	
	std::cout << "Starting up chat and interactive bot" << std::endl;

	// Setup the config
	DndConfigPtr config_interactive = std::make_shared<DndConfig>();
	ChatConfigPtr config_chat = std::make_shared<ChatConfig>();


	int err = 0;
	if ((err = config_interactive->Init("dndpanelconfig.json")))
	{
		Logger::Error("Failed to read interactive config.");
		return err;
	}
	
	if ((err = config_chat->Init("chatconfig.json")))
	{
		Logger::Error("Failed to read chat config.");
		return err;
	}

	AuthPtr interactiveAuth = std::make_shared<Auth>();
	AuthPtr chatAuth = std::make_shared<Auth>();

	std::cout << "Preping interactive" << std::endl;
	// Check auth
	if ((err = interactiveAuth->EnsureAuth(config_interactive)))
	{
		Logger::Error("Failed setup for interactive auth.");
		return err;
	}

	std::cout << "Press enter after interactive is started" << std::endl;
	std::cin.get();

	int channelToConnectTo;

	std::ifstream inFile;

	inFile.open("channelcache.txt");
	if (inFile) {
		inFile >> channelToConnectTo;
		std::cout << "Found a cached channel would you like to use it (y/n): " << channelToConnectTo << std::endl;
		inFile.close();
	}

	char response;
	std::cin >> response;
	if (response == 'y')
	{
		std::cout << "Keeping cached channel." << std::endl;
	}
	else
	{
		std::cout << "Insert channel to connect to with chat:";
		std::cin >> channelToConnectTo;
	}

	std::ofstream myfile("channelcache.txt");
	if (myfile.is_open())
	{
		myfile << channelToConnectTo;
		myfile.close();
	}
	

	std::cout << "Preping chat for channel number:" << channelToConnectTo << std::endl;

	// Check auth
	if ((err = chatAuth->EnsureAuth(config_chat)))
	{
		Logger::Error("Failed setup chat auth.");
		return err;
	}

    DndRunnerPtr interactiveRunner = std::make_shared<DndRunner>();
	ChatRunnerPtr chatRunner = std::make_shared<ChatRunner>();

	
	std::thread interactive(runPanel, interactiveRunner, interactiveAuth, config_interactive);
	std::thread chat(runChat, chatRunner, chatAuth, config_chat, channelToConnectTo);
	
	while (true)
	{
		//run forever
	}
}

std::string userstatelocation = "userstate.json";

void saveFile(rapidjson::Document& d)
{
	FILE* fp = fopen(userstatelocation.c_str(), "wb"); // non-Windows use "w"
	char writeBuffer[65536];
	FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));
	Writer<FileWriteStream> writer(os);
	d.Accept(writer);
	fclose(fp);
}

void prepUserState(chat_session_internal* sessionInternal)
{
	FILE* fp = fopen(userstatelocation.c_str() , "rb"); // non-Windows use "r"
	
	if (fp == nullptr)
	{
		sessionInternal->usersState.SetObject();
		return;
	}
	

	char readBuffer[65536];
	FileReadStream is(fp, readBuffer, sizeof(readBuffer));
	sessionInternal->usersState.ParseStream(is);
	fclose(fp);

	if (sessionInternal->usersState.IsNull())
	{
		sessionInternal->usersState.SetObject();
	}
}

int ChatRunner::Run(AuthPtr auth, ChatConfigPtr config, int channelToConnectTo)
{
	int err = 0;

	m_auth = auth;
	m_config = config;
	
	
	
	// Setup chat
	if ((err = chat_open_session(&m_session)))
	{
		Logger::Error("Failed to setup chat!");
		return err;
	}
	chat_session_internal* sessionInternal = reinterpret_cast<chat_session_internal*>(m_session);

	std::vector<std::pair<std::string, int>> pairs;
	for (auto itr = config->levels.begin(); itr != config->levels.end(); ++itr)
		pairs.push_back(*itr);

	sort(pairs.begin(), pairs.end(), [=](std::pair<std::string, int>& a, std::pair<std::string, int>& b)
	{
		return a.second < b.second;
	}
	);
	
	sessionInternal->levels = pairs;
	
	sessionInternal->m_auth = auth;
	sessionInternal->chatToConnect = channelToConnectTo;

	// Setup handlers
	if ((err = SetupHandlers()))
	{
		Logger::Error("Failed to setup handlers");
		return err;
	}

	// Connect
	if ((err = chat_connect(m_session, m_auth->authToken.c_str(), m_config->InteractiveId.c_str(), m_config->ShareCode.c_str(), true)))
	{
		Logger::Error("Failed to connect to chat!");
		return err;
	}

	// Run! (like this was a game loop)
	high_clock::time_point lastTickRun = high_clock::now();
	
	
	prepUserState(sessionInternal);
	for (;;)
	{
		// Run interactive
		if ((err = chat_run(m_session, 500)))
		{
			break;
		}

		// Run the update logic.
		high_clock::time_point now = high_clock::now();
		lastTickRun = now;

		// Save the local state to disk
		
		saveFile(sessionInternal->usersState);

		// Sleepy time.
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}

	return err;
}

int DndRunner::Run(AuthPtr auth, DndConfigPtr config)
{
	int err = 0;

	m_auth = auth;
	m_config = config;

    // Create our logic class.
    m_panelLogic = std::make_shared<PanelLogic>(GetSharedPtr<DndRunner>());

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
    if ((err = interactive_connect(m_session, m_auth->authToken.c_str(), m_config->InteractiveId.c_str(), m_config->ShareCode.c_str(), true)))
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

void DndRunner::ParticipantsChangedHandler(interactive_participant_action action, const interactive_participant* participant)
{
    std::stringstream ss;
    ss << "User [" << participant->userId << "] " << std::string(participant->userName, participant->usernameLength) << (action == interactive_participant_action::participant_join ? " joined " : " left ") << "\n";
    Logger::Info(ss.str());
}

void ChatRunner::ParticipantsChangedHandler(chat_participant_action action, const chat_participant* participant)
{
	std::stringstream ss;
	ss << "User [" << participant->userId << "] " << std::string(participant->userName, participant->usernameLength) << (action == chat_participant_action::participant_join_c ? " joined " : " left ") << "\n";
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

int chat_set_session_context(chat_session session, void* context)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	chat_session_internal* sessionInternal = reinterpret_cast<chat_session_internal*>(session);
	sessionInternal->callerContext = context;

	return MIXER_OK;
}

int chat_set_participants_changed_handler(chat_session session, on_participants_changed_c onParticipantsChanged)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	chat_session_internal* sessionInternal = reinterpret_cast<chat_session_internal*>(session);
	sessionInternal->onParticipantsChanged = onParticipantsChanged;

	return MIXER_OK;
}

// Handler registration
int chat_set_error_handler(chat_session session, on_error onError)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	chat_session_internal* sessionInternal = reinterpret_cast<chat_session_internal*>(session);
	sessionInternal->onError = onError;

	return MIXER_OK;
}

void participants_changed_handler_c(void* context, chat_session session, chat_participant_action action, const chat_participant* participant)
{
	if (context)
	{
		ChatRunner* runner = (ChatRunner*)context;
		runner->ParticipantsChangedHandler(action, participant);
	}
}

int chat_set_input_handler(chat_session session, on_input onInput)
{
	if (nullptr == session)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	chat_session_internal* sessionInternal = reinterpret_cast<chat_session_internal*>(session);
	sessionInternal->onInput = onInput;

	return MIXER_OK;
}

int ChatRunner::SetupHandlers()
{
	int err = 0;

	// Set this object as the context.
	err = chat_set_session_context(m_session, this);
	if (err) return err;

	// Register a callback for errors.
	err = chat_set_error_handler(m_session, handle_error);
	if (err) return err;

	err = chat_set_participants_changed_handler(m_session, participants_changed_handler_c);
	if (err) return err;

	// Register a callback for button presses.
	err = chat_set_input_handler(m_session, input_handler);
	if (err) return err;

	return err;
}