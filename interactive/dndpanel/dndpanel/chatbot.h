#pragma once

#include "rapidjson/document.h"

#include "../Chat/ChatHandler.h"
#include "../Quests/QuestManager.h"
#include "../Classes/classes.h"
#include "../Professions/jobs.h"
#include "../Chat/chat_event_internal.h"

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

		void verifyUser(std::string Name);

		void incrementXp(std::string Name, int xpGain);

		int getViewerCurrentXp(std::string Name);

		std::map<std::string, int> getViewerStats(std::string Name);

		int getLevel(std::string Name);

		std::string getClass(std::string Name);

		std::string getJob(std::string Name);

		void chatTest(rapidjson::Document& doc);

		void chatWhisperTest(rapidjson::Document& doc);

		void chatDeleteTest(rapidjson::Document& doc);

		void xp(rapidjson::Document& doc);

		void level(rapidjson::Document& doc);
		
		void commands(rapidjson::Document& doc);

		void cclass(rapidjson::Document& doc);

		void selectClass(rapidjson::Document& doc);

		void job(rapidjson::Document& doc);

		void listJobs(rapidjson::Document& doc);
		
		void selectJob(rapidjson::Document& doc);

		void listClasses(rapidjson::Document& doc);

		void listQuests(rapidjson::Document& doc);

		void describeQuest(rapidjson::Document& doc);

		void startQuest(rapidjson::Document& doc);

		void joinQuest(rapidjson::Document& doc);

		void StartQuest(Quests::QuestPtr, std::string);

		void RunQuest(Quests::QuestPtr);

		int route_method(rapidjson::Document& doc);
		int route_event(rapidjson::Document& doc);

		int chat_run(unsigned int maxEventsToProcess);

		// Helper lists
		Professions::jobslistPtr jobList;
		Classes::classInfoListPtr classList;
		Quests::QuestManagerPtr questManager;
		
		// Game Info
		std::vector<std::pair<std::string, int>> levels;

		// User Information
		rapidjson::Document usersState;

		// Chat Handler
		Chat::ChatHandlerPtr chatHandler;
	};
}