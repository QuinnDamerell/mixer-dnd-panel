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

void ChatBot::incrementXp(chat_session_internal& session, std::string Name, int xpGain)
{
	verifyUser(session, Name);
	for (auto& v : session.usersState["Users"].GetArray())
	{
		if (v["Name"].GetString() == Name)
		{
			v["XP"] = v["XP"].GetInt() + xpGain;
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

	
	DnDPanel::Logger::Info(std::string("Queueing method: ") + mixer_internal::jsonStringify(*doc));
	std::shared_ptr<rpc_method_event> methodEvent = std::make_shared<rpc_method_event>(std::move(doc));
	std::unique_lock<std::mutex> queueLock(session.outgoingMutex);
	session.outgoingEvents.emplace(methodEvent);
	session.outgoingCV.notify_one();
}

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

	}

	
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