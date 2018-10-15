#pragma once

#include "rapidjson/document.h"

#include "chat_session_internal.h"

namespace ChatBot
{
	DECLARE_SMARTPOINTER(Bot);
	class Bot :
		public SharedFromThis
	{
	public:

		Bot();

		void Init();

		bool arrayContains(rapidjson::Value& arrayToCheck, std::string value);

		void verifyUser(ChatSession::chat_session_internal& session, std::string Name);

		void incrementXp(ChatSession::chat_session_internal& session, std::string Name, int xpGain);

		int getXp(ChatSession::chat_session_internal& session, std::string Name);

		std::string getLevel(ChatSession::chat_session_internal& session, std::string Name);

		int handle_chat_message(ChatSession::chat_session_internal& session, rapidjson::Document& doc);

		int handle_reply(ChatSession::chat_session_internal& session, rapidjson::Document& doc);

		int handle_user_join(ChatSession::chat_session_internal& session, rapidjson::Document& doc);

		int handle_user_leave(ChatSession::chat_session_internal& session, rapidjson::Document& doc);

		void sendMessage(ChatSession::chat_session_internal& session, std::string message);

		void sendWhisper(ChatSession::chat_session_internal& session, std::string message, std::string target);

		void deleteMessage(ChatSession::chat_session_internal& session, std::string id);

		void chatTest(ChatSession::chat_session_internal& session, rapidjson::Document& doc);

		void chatWhisperTest(ChatSession::chat_session_internal& session, rapidjson::Document& doc);

		void chatDeleteTest(ChatSession::chat_session_internal& session, rapidjson::Document& doc);

		void xp(ChatSession::chat_session_internal& session, rapidjson::Document& doc);

		void level(ChatSession::chat_session_internal& session, rapidjson::Document& doc);
		
		void commands(ChatSession::chat_session_internal& session, rapidjson::Document& doc);

		std::map<std::string, std::function<void(ChatSession::chat_session_internal&, rapidjson::Document&)>> funcMap;

	};
}