#include "chatbot.h"
#include "rapidjson/document.h"

using namespace RAPIDJSON_NAMESPACE;
using namespace ChatUtil;

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
		session.usersState["Users"].PushBack(o, session.usersState.GetAllocator());
	}
}

void ChatBot::incrementXp(chat_session_internal& session, std::string Name)
{
	verifyUser(session, Name);
	for (auto& v : session.usersState["Users"].GetArray())
	{
		if (v["Name"].GetString() == Name)
		{
			v["XP"] = v["XP"].GetInt() + 1;
		}
	}
}

// The events that matter
int ChatBot::handle_chat_message(chat_session_internal& session, rapidjson::Document& doc)
{
	(doc);
	std::string username = doc["data"]["user_name"].GetString();
	incrementXp(session, username);
	return 0;
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