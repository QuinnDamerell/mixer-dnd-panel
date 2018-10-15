#include "chatbot.h"
#include "rapidjson/document.h"
#include <string>
#include "logger.h"
#include <functional>
#include "chatutil.h"

using namespace std;
using namespace RAPIDJSON_NAMESPACE;
using namespace ChatUtil;
using namespace ChatBot;
using namespace ChatSession;


Bot* saved_bot;

void Bot::chatTest(chat_session_internal& session, rapidjson::Document& doc)
{
	sendMessage(session, "This should print in chat");
}

void chatTest_wrapper(chat_session_internal& session, rapidjson::Document& doc)
{
	saved_bot->chatTest(session, doc);
}

void Bot::chatWhisperTest(chat_session_internal& session, rapidjson::Document& doc)
{
	std::string username = doc["data"]["user_name"].GetString();
	sendWhisper(session, "This should whisper the person testing", username);
}

void chatWhisperTest_wrapper(chat_session_internal& session, rapidjson::Document& doc)
{
	saved_bot->chatWhisperTest(session, doc);
}

void Bot::chatDeleteTest(chat_session_internal& session, rapidjson::Document& doc)
{
	deleteMessage(session, doc["data"]["id"].GetString());
}

void chatDeleteTest_wrapper(chat_session_internal& session, rapidjson::Document& doc)
{
	saved_bot->chatDeleteTest(session, doc);
}

void Bot::xp(chat_session_internal& session, rapidjson::Document& doc)
{
	std::string username = doc["data"]["user_name"].GetString();
	sendWhisper(session, username + " has: " + to_string(getXp(session, username)) + " xp.", username);
	deleteMessage(session, doc["data"]["id"].GetString());
}

void xp_wrapper(chat_session_internal& session, rapidjson::Document& doc)
{
	saved_bot->xp(session, doc);
}

void Bot::level(chat_session_internal& session, rapidjson::Document& doc)
{
	std::string username = doc["data"]["user_name"].GetString();
	sendWhisper(session, username + " is level: " + getLevel(session, username) + ".", username);
	deleteMessage(session, doc["data"]["id"].GetString());
}

void level_wrapper(chat_session_internal& session, rapidjson::Document& doc)
{
	saved_bot->level(session, doc);
}

void Bot::commands(chat_session_internal& session, rapidjson::Document& doc)
{
	std::string result = "Commands are:";

	bool isFirst = true;
	for (std::map<std::string, std::function<void(chat_session_internal&, rapidjson::Document&)>>::iterator iter = funcMap.begin(); iter != funcMap.end(); ++iter)
	{
		std::string key = iter->first;

		if (!isFirst)
		{
			result += ", " + key;
		}

	}

	std::string username = doc["data"]["user_name"].GetString();
	sendWhisper(session, result, username);
	deleteMessage(session, doc["data"]["id"].GetString());
}

void commands_wrapper(chat_session_internal& session, rapidjson::Document& doc)
{
	saved_bot->commands(session, doc);
}

Bot::Bot()
{
	Init();

	funcMap.insert(std::pair<std::string, std::function<void(chat_session_internal&, rapidjson::Document&)>>("!chatTest", chatTest_wrapper));
	funcMap.insert(std::pair<std::string, std::function<void(chat_session_internal&, rapidjson::Document&)>>("!chatWhisperTest", chatWhisperTest_wrapper));
	funcMap.insert(std::pair<std::string, std::function<void(chat_session_internal&, rapidjson::Document&)>>("!chatDeleteTest", chatDeleteTest_wrapper));
	funcMap.insert(std::pair<std::string, std::function<void(chat_session_internal&, rapidjson::Document&)>>("!xp", xp_wrapper));
	funcMap.insert(std::pair<std::string, std::function<void(chat_session_internal&, rapidjson::Document&)>>("!level", level_wrapper));
	funcMap.insert(std::pair<std::string, std::function<void(chat_session_internal&, rapidjson::Document&)>>("!commands", commands_wrapper));
}

void Bot::Init()
{
	saved_bot = this;
}

// The events that matter
int Bot::handle_chat_message(chat_session_internal& session, rapidjson::Document& doc)
{
	(doc);
	
	//Credit xp for message
	std::string username = doc["data"]["user_name"].GetString();
	incrementXp(session, username, 1);

	for (auto& v : doc["data"]["message"]["message"].GetArray())
	{
		std::string found_text = v.GetObject()["text"].GetString();
		map<std::string, std::function<void(chat_session_internal&, rapidjson::Document&)>>::iterator it = funcMap.find(found_text);
		std::function<void(chat_session_internal&, rapidjson::Document&)> b3;
		if (it != funcMap.end())
		{
			//element found;
			b3 = it->second;
			b3(session, doc);
		}
	}
	//Route message
	
	
	return 0;
}

bool Bot::arrayContains(rapidjson::Value& arrayToCheck, std::string value)
{
	for (auto& v : arrayToCheck.GetArray())
	{
		if (v["Name"].GetString() == value)
		{
			return true;
		}
	}

	return false;
}

void Bot::verifyUser(chat_session_internal& session, std::string Name)
{
	if (!session.usersState.HasMember("Users"))
	{
		Value a(kArrayType);
		session.usersState.AddMember("Users", a, session.usersState.GetAllocator());
	}
	if (!arrayContains(session.usersState["Users"], Name))
	{
		Value o(kObjectType);
		o.AddMember("Name", Name, session.usersState.GetAllocator());
		o.AddMember("XP", 0, session.usersState.GetAllocator());
		o.AddMember("Strength", 0, session.usersState.GetAllocator());
		o.AddMember("Dexterity", 0, session.usersState.GetAllocator());
		o.AddMember("Constitution", 0, session.usersState.GetAllocator());
		o.AddMember("Intelligence", 0, session.usersState.GetAllocator());
		o.AddMember("Wisdom", 0, session.usersState.GetAllocator());
		o.AddMember("Charisma", 0, session.usersState.GetAllocator());
		o.AddMember("Class", "No Class", session.usersState.GetAllocator());
		o.AddMember("ClassLevel", 0, session.usersState.GetAllocator());
		o.AddMember("Job", "No Job", session.usersState.GetAllocator());
		o.AddMember("JobLevel", 0, session.usersState.GetAllocator());
		o.AddMember("ViewerLevel", "1", session.usersState.GetAllocator());
		session.usersState["Users"].PushBack(o, session.usersState.GetAllocator());
	}
}

int Bot::getXp(chat_session_internal& session, std::string Name)
{
	verifyUser(session, Name);
	for (auto& v : session.usersState["Users"].GetArray())
	{
		if (v["Name"].GetString() == Name)
		{
			return v["XP"].GetInt();
		}
	}
}

std::string Bot::getLevel(chat_session_internal& session, std::string Name)
{
	verifyUser(session, Name);
	int xp = getXp(session, Name);
	for (std::vector<std::pair<std::string, int>>::iterator it = session.levels.begin(); it != session.levels.end(); ++it)
	{
		std::string name = it->first;
		int levelMax = it->second;
		if (xp < levelMax)
		{
			return name;
		}
	}

	return "No Level";
}

void Bot::incrementXp(chat_session_internal& session, std::string Name, int xpGain)
{
	verifyUser(session, Name);
	for (auto& v : session.usersState["Users"].GetArray())
	{
		if (v["Name"].GetString() == Name)
		{
			v["XP"] = v["XP"].GetInt() + xpGain;
			v["ViewerLevel"].SetString(getLevel(session, Name), session.usersState.GetAllocator());
			return;
		}
	}
}

int message_id = 1000;

void Bot::sendMessage(chat_session_internal& session, std::string message)
{
	std::shared_ptr<rapidjson::Document> doc(std::make_shared<rapidjson::Document>());
	doc->SetObject();
	rapidjson::Document::AllocatorType& allocator = doc->GetAllocator();

	
	doc->AddMember("type", "method", allocator);
	doc->AddMember("method", "msg", allocator);
	Value a(kArrayType);
	Value chatMessage(message, allocator);
	a.PushBack(chatMessage, allocator);
	doc->AddMember("arguments", a, allocator);
	doc->AddMember("id", message_id, allocator);
	message_id++;

	
	//DnDPanel::Logger::Info(std::string("Queueing method: ") + mixer_internal::jsonStringify(*doc));
	std::shared_ptr<rpc_method_event> methodEvent = std::make_shared<rpc_method_event>(std::move(doc));
	std::unique_lock<std::mutex> queueLock(session.outgoingMutex);
	session.outgoingEvents.emplace(methodEvent);
	session.outgoingCV.notify_one();
}

void Bot::sendWhisper(chat_session_internal& session, std::string message, std::string target)
{
	std::shared_ptr<rapidjson::Document> doc(std::make_shared<rapidjson::Document>());
	doc->SetObject();
	rapidjson::Document::AllocatorType& allocator = doc->GetAllocator();


	doc->AddMember("type", "method", allocator);
	doc->AddMember("method", "whisper", allocator);
	Value a(kArrayType);
	Value chatMessage(message, allocator);
	Value chatTarget(target, allocator);
	a.PushBack(chatTarget, allocator);
	a.PushBack(chatMessage, allocator);
	doc->AddMember("arguments", a, allocator);
	doc->AddMember("id", message_id, allocator);
	message_id++;


	//DnDPanel::Logger::Info(std::string("Queueing method: ") + mixer_internal::jsonStringify(*doc));
	std::shared_ptr<rpc_method_event> methodEvent = std::make_shared<rpc_method_event>(std::move(doc));
	std::unique_lock<std::mutex> queueLock(session.outgoingMutex);
	session.outgoingEvents.emplace(methodEvent);
	session.outgoingCV.notify_one();
}

void Bot::deleteMessage(chat_session_internal& session, std::string id)
{
	std::shared_ptr<rapidjson::Document> doc(std::make_shared<rapidjson::Document>());
	doc->SetObject();
	rapidjson::Document::AllocatorType& allocator = doc->GetAllocator();


	doc->AddMember("type", "method", allocator);
	doc->AddMember("method", "deleteMessage", allocator);
	Value a(kArrayType);
	Value chatMessage(id, allocator);
	a.PushBack(chatMessage, allocator);
	doc->AddMember("arguments", a, allocator);
	doc->AddMember("id", message_id, allocator);
	message_id++;


	//DnDPanel::Logger::Info(std::string("Queueing method: ") + mixer_internal::jsonStringify(*doc));
	std::shared_ptr<rpc_method_event> methodEvent = std::make_shared<rpc_method_event>(std::move(doc));
	std::unique_lock<std::mutex> queueLock(session.outgoingMutex);
	session.outgoingEvents.emplace(methodEvent);
	session.outgoingCV.notify_one();
}

int Bot::handle_reply(chat_session_internal& session, rapidjson::Document& doc)
{
	(doc);
	std::string methodJson = mixer_internal::jsonStringify(doc);
	return 0;
}

int Bot::handle_user_join(chat_session_internal& session, rapidjson::Document& doc)
{
	(doc);
	std::string username = doc["data"]["username"].GetString();
	verifyUser(session, username);

	return 0;
}

int Bot::handle_user_leave(chat_session_internal& session, rapidjson::Document& doc)
{
	(doc);
	std::string methodJson = mixer_internal::jsonStringify(doc);
	return 0;
}