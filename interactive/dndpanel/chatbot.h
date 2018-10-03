#pragma once

#include "rapidjson/document.h"
#include "chatutil.h"

namespace ChatBot
{
	bool arrayContains(rapidjson::Value& arrayToCheck, std::string value);

	void verifyUser(ChatUtil::chat_session_internal& session, std::string Name);

	void incrementXp(ChatUtil::chat_session_internal& session, std::string Name, int xpGain);

	int handle_chat_message(ChatUtil::chat_session_internal& session, rapidjson::Document& doc);

	int handle_reply(ChatUtil::chat_session_internal& session, rapidjson::Document& doc);

	int handle_user_join(ChatUtil::chat_session_internal& session, rapidjson::Document& doc);

	int handle_user_leave(ChatUtil::chat_session_internal& session, rapidjson::Document& doc);

	void sendMessage(ChatUtil::chat_session_internal& session, std::string message);
}