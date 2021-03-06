#include "chatbot.h"
#include "rapidjson/document.h"
#include <string>
#include "logger.h"

using namespace std;
using namespace RAPIDJSON_NAMESPACE;
using namespace ChatUtil;

// The events that matter
int ChatBot::handle_chat_message(chat_session_internal& session, rapidjson::Document& doc)
{
	(doc);
	std::string username = doc["data"]["user_name"].GetString();
	incrementXp(session, username, 1);

	for (auto& v : doc["data"]["message"]["message"].GetArray())
	{
		std::string found_text = v.GetObject()["text"].GetString();

		if (found_text == "!echo")
		{
			sendMessage(session, "Echo echo echo echo echo");
		}

		if (found_text == "!xp")
		{
			sendMessage(session, username + " has: " + to_string(getXp(session, username)) + " xp.");
		}

		if (found_text == "!level")
		{
			sendMessage(session, username + " is level: " + getLevel(session, username) + ".");
		}
	}

	return 0;
}

bool ChatBot::arrayContains(rapidjson::Value& arrayToCheck, std::string value)
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

void ChatBot::verifyUser(chat_session_internal& session, std::string Name)
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

int ChatBot::getXp(chat_session_internal& session, std::string Name)
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

std::string ChatBot::getLevel(chat_session_internal& session, std::string Name)
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

void ChatBot::incrementXp(chat_session_internal& session, std::string Name, int xpGain)
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

void ChatBot::sendMessage(chat_session_internal& session, std::string message)
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



int ChatBot::handle_reply(chat_session_internal& session, rapidjson::Document& doc)
{
	(doc);
	std::string methodJson = mixer_internal::jsonStringify(doc);
	return 0;
}

int ChatBot::handle_user_join(chat_session_internal& session, rapidjson::Document& doc)
{
	(doc);
	std::string username = doc["data"]["username"].GetString();
	verifyUser(session, username);

	return 0;
}

int ChatBot::handle_user_leave(chat_session_internal& session, rapidjson::Document& doc)
{
	(doc);
	std::string methodJson = mixer_internal::jsonStringify(doc);
	return 0;
}