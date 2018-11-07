#include "chatbot.h"

#include "rapidjson/document.h"
#include <string>
#include "logger.h"
#include <functional>
#include "internal/json.h"

using namespace std;
using namespace RAPIDJSON_NAMESPACE;
using namespace Chat;
using namespace ChatBot;


Bot* saved_bot;

void Bot::chatTest(rapidjson::Document& doc)
{
	chatHandler->sendMessage("This should print in chat");
}

void chatTest_wrapper(rapidjson::Document& doc)
{
	saved_bot->chatTest(doc);
}

void Bot::chatWhisperTest(rapidjson::Document& doc)
{
	std::string username = doc["data"]["user_name"].GetString();
	chatHandler->sendWhisper("This should whisper the person testing", username);
}

void chatWhisperTest_wrapper(rapidjson::Document& doc)
{
	saved_bot->chatWhisperTest(doc);
}

void Bot::chatDeleteTest(rapidjson::Document& doc)
{
	chatHandler->deleteMessage(doc["data"]["id"].GetString());
}

void chatDeleteTest_wrapper(rapidjson::Document& doc)
{
	saved_bot->chatDeleteTest(doc);
}

void Bot::xp(rapidjson::Document& doc)
{
	std::string username = doc["data"]["user_name"].GetString();
	chatHandler->sendWhisper(username + " has: " + to_string(getViewerCurrentXp(username)) + " xp.", username);
	chatHandler->deleteMessage(doc["data"]["id"].GetString());
}

void xp_wrapper(rapidjson::Document& doc)
{
	saved_bot->xp(doc);
}

void Bot::level(rapidjson::Document& doc)
{
	std::string username = doc["data"]["user_name"].GetString();
	chatHandler->sendWhisper(username + " is level: " + to_string(getLevel(username)) + ".", username);
	chatHandler->deleteMessage(doc["data"]["id"].GetString());
}

void level_wrapper(rapidjson::Document& doc)
{
	saved_bot->level(doc);
}

void Bot::commands(rapidjson::Document& doc)
{
	std::string result = "Commands are:";

	bool isFirst = true;
	for (std::map<std::string, std::function<void(rapidjson::Document&)>>::iterator iter = chatHandler->funcMap.begin(); iter != chatHandler->funcMap.end(); ++iter)
	{
		std::string key = iter->first;

		if (!isFirst)
		{
			result += ", " + key;
		}
		else
		{
			result += key;
			isFirst = false;
		}

	}

	std::string username = doc["data"]["user_name"].GetString();
	chatHandler->sendWhisper(result, username);
	chatHandler->deleteMessage(doc["data"]["id"].GetString());
}

void commands_wrapper(rapidjson::Document& doc)
{
	saved_bot->commands(doc);
}

void Bot::cclass(rapidjson::Document& doc)
{
	std::string username = doc["data"]["user_name"].GetString();
	chatHandler->sendWhisper(username + " is class: " + getClass(username) + ".", username);
	chatHandler->deleteMessage(doc["data"]["id"].GetString());
}

void class_wrapper(rapidjson::Document& doc)
{
	saved_bot->cclass(doc);
}

void Bot::job(rapidjson::Document& doc)
{
	std::string username = doc["data"]["user_name"].GetString();
	chatHandler->sendWhisper(username + " is job: " + getJob(username) + ".", username);
	chatHandler->deleteMessage(doc["data"]["id"].GetString());
}

void job_wrapper(rapidjson::Document& doc)
{
	saved_bot->job(doc);
}

void Bot::listJobs(rapidjson::Document& doc)
{
	std::string result = "";

	bool isFirst = true;
	for (auto &job : jobList->jobs) // access by reference to avoid copying
	{
		std::string key = job->BaseName;

		if (!isFirst)
		{
			result += ", " + key;
		}
		else
		{
			result += key;
			isFirst = false;
		}
	}

	std::string username = doc["data"]["user_name"].GetString();
	chatHandler->sendWhisper(username + " all jobs are: " + result + ".", username);
	chatHandler->deleteMessage(doc["data"]["id"].GetString());
}

void listJobs_wrapper(rapidjson::Document& doc)
{
	saved_bot->listJobs(doc);
}

void Bot::listClasses(rapidjson::Document& doc)
{
	std::string result = "";

	bool isFirst = true;
	for (auto &classInfo : classList->classInfos) // access by reference to avoid copying
	{
		std::string key = classInfo->BaseName;

		if (!isFirst)
		{
			result += ", " + key;
		}
		else
		{
			result += key;
			isFirst = false;
		}
	}

	std::string username = doc["data"]["user_name"].GetString();
	chatHandler->sendWhisper(username + " all classes are: " + result + ".", username);
	chatHandler->deleteMessage(doc["data"]["id"].GetString());
}

void listClasses_wrapper(rapidjson::Document& doc)
{
	saved_bot->listClasses(doc);
}

void Bot::selectJob(rapidjson::Document& doc)
{
	std::string username = doc["data"]["user_name"].GetString();

	if (getJob(username) != "No Job")
	{
		chatHandler->sendWhisper(username + "You already have a job which is:" + getJob(username), username);
	}
	else
	{
		for (auto& v : doc["data"]["message"]["message"].GetArray())
		{
			istringstream iss(v.GetObject()["text"].GetString());
			vector<string> tokens{ istream_iterator<string>{iss},
						  istream_iterator<string>{} };

			std::string selectedJob = tokens[1];
			bool foundJob = false;
			for (auto &job : jobList->jobs) // access by reference to avoid copying
			{
				std::string key = job->BaseName;

				if (key == selectedJob)
				{
					for (auto& v : usersState["Users"].GetArray())
					{
						if (v["Name"].GetString() == username)
						{
							v["BaseJob"].SetString(StringRef(key.c_str()), doc.GetAllocator());
							chatHandler->sendWhisper(username + "Setting your job to:" + key, username);
							foundJob = true;
						}
					}
				}
			}
			if (!foundJob)
			{
				chatHandler->sendWhisper(username + "The job you selected does not exist. Check listJobs to see to find a job.", username);
			}

		}
	}
	
	chatHandler->deleteMessage(doc["data"]["id"].GetString());
}

void selectJob_wrapper(rapidjson::Document& doc)
{
	saved_bot->selectJob(doc);
}

void Bot::selectClass(rapidjson::Document& doc)
{
	std::string username = doc["data"]["user_name"].GetString();

	if (getClass(username) != "No Class")
	{
		chatHandler->sendWhisper(username + "You already have a class which is:" + getClass(username), username);
	}
	else
	{
		for (auto& v : doc["data"]["message"]["message"].GetArray())
		{
			istringstream iss(v.GetObject()["text"].GetString());
			vector<string> tokens{ istream_iterator<string>{iss},
						  istream_iterator<string>{} };

			std::string selectedClass = tokens[1];
			bool foundClass = false;
			for (auto &classInfo : classList->classInfos) // access by reference to avoid copying
			{
				std::string key = classInfo->BaseName;

				if (key == selectedClass)
				{
					for (auto& v : usersState["Users"].GetArray())
					{
						if (v["Name"].GetString() == username)
						{
							v["BaseClass"].SetString(StringRef(key.c_str()), doc.GetAllocator());
							auto stats = v["Stats"].GetObject();
							stats[classInfo->MainStat].SetInt(1);
							
							chatHandler->sendWhisper(username + "Setting your class to:" + key, username);
							foundClass = true;
						}
					}
				}
			}
			if (!foundClass)
			{
				chatHandler->sendWhisper(username + "The class you selected does not exist. Check listClasss to see to find a class.", username);
			}

		}
	}

	chatHandler->deleteMessage(doc["data"]["id"].GetString());
}

void selectClass_wrapper(rapidjson::Document& doc)
{
	saved_bot->selectClass(doc);
}

void Bot::listQuests(rapidjson::Document& doc)
{
	std::string result = "";

	bool isFirst = true;
	for (auto &quest : questManager->QuestList) // access by reference to avoid copying
	{
		std::string key = quest->Name;

		if (!isFirst)
		{
			result += ", " + key;
		}
		else
		{
			result += key;
			isFirst = false;
		}
	}

	std::string username = doc["data"]["user_name"].GetString();
	chatHandler->sendWhisper(username + " all quests are: " + result + ".", username);
	chatHandler->deleteMessage(doc["data"]["id"].GetString());
}

void listQuests_wrapper(rapidjson::Document& doc)
{
	saved_bot->listQuests(doc);
}

void Bot::describeQuest(rapidjson::Document& doc)
{
	std::string username = doc["data"]["user_name"].GetString();

	
	for (auto& v : doc["data"]["message"]["message"].GetArray())
	{
		std::string input = v.GetObject()["text"].GetString();
		std::string selectedQuest = input.substr(input.find("!describeQuest ") + 15);
		std::cout << input << std::endl;
		std::cout << selectedQuest << std::endl;
		bool foundJob = false;
		for (auto &quest : questManager->QuestList) // access by reference to avoid copying
		{
			std::string key = quest->Name;

			if (key == selectedQuest)
			{
				chatHandler->sendWhisper(username + "Quest Name:" + key + ". Description:" + quest->Description + " and " + std::to_string(quest->Slots) + " people can go on the quest", username);
			}
		}
	}

	chatHandler->deleteMessage(doc["data"]["id"].GetString());
}

void describeQuest_wrapper(rapidjson::Document& doc)
{
	saved_bot->describeQuest(doc);
}

void Bot::startQuest(rapidjson::Document& doc)
{
	std::string username = doc["data"]["user_name"].GetString();

	if (questManager->isQuestActive)
	{
		chatHandler->sendWhisper(username + "A quest is already active", username);
	}
	else
	{
		for (auto& v : doc["data"]["message"]["message"].GetArray())
		{
			std::string input = v.GetObject()["text"].GetString();
			std::string selectedQuest = input.substr(input.find("!startQuest ") + 12);
			std::cout << input << std::endl;
			std::cout << selectedQuest << std::endl;
			bool foundJob = false;
			for (auto &quest : questManager->QuestList) // access by reference to avoid copying
			{
				std::string key = quest->Name;

				if (key == selectedQuest)
				{
					std::string amountToJoin = "Anybody can join.";
					if (quest->Slots != 0)
					{
						amountToJoin = "Up to " + std::to_string(quest->Slots) + " people can join.";
					}
					chatHandler->sendMessage(username + " has started the quest " + quest->Name + ". To join the quest do !joinQuest. " + amountToJoin);
					StartQuest(quest, username);
				}
			}
			if (questManager->isQuestActive != true)
			{
				chatHandler->sendWhisper(username + " could not find a quest with name : " + selectedQuest + ". To start a quest run !listQuests to try to find the right name", username);
			}
		}
	}

	chatHandler->deleteMessage(doc["data"]["id"].GetString());
}

void startQuest_wrapper(rapidjson::Document& doc)
{
	saved_bot->startQuest(doc);
}

void Bot::joinQuest(rapidjson::Document& doc)
{
	std::string username = doc["data"]["user_name"].GetString();

	if (!questManager->isQuestActive)
	{
		chatHandler->sendWhisper(username + "A quest is not active start your own", username);
	}
	else if (questManager->currentAdventurers == questManager->maxAdventurers && questManager->maxAdventurers != 0)
	{
		chatHandler->sendWhisper(username + "Max adventures selected. Join the next one!", username);
	}
	else if (questManager->questStarted)
	{
		chatHandler->sendWhisper(username + "The quest is already active. Join the next one!", username);
	}
	else 
	{
		questManager->adventurers.emplace_back(username);
		questManager->currentAdventurers += 1;
		chatHandler->sendWhisper(username + "You have joined the quest!", username);
	}

	chatHandler->deleteMessage(doc["data"]["id"].GetString());
}

void joinQuest_wrapper(rapidjson::Document& doc)
{
	saved_bot->joinQuest(doc);
}

Bot::Bot()
{
	Init();
	chatHandler = make_shared<ChatHandler>();

	chatHandler->funcMap.insert(std::pair<std::string, std::function<void(rapidjson::Document&)>>("!chatTest", chatTest_wrapper));
	chatHandler->funcMap.insert(std::pair<std::string, std::function<void(rapidjson::Document&)>>("!chatWhisperTest", chatWhisperTest_wrapper));
	chatHandler->funcMap.insert(std::pair<std::string, std::function<void(rapidjson::Document&)>>("!chatDeleteTest", chatDeleteTest_wrapper));
	chatHandler->funcMap.insert(std::pair<std::string, std::function<void(rapidjson::Document&)>>("!xp", xp_wrapper));
	chatHandler->funcMap.insert(std::pair<std::string, std::function<void(rapidjson::Document&)>>("!level", level_wrapper));
	chatHandler->funcMap.insert(std::pair<std::string, std::function<void(rapidjson::Document&)>>("!commands", commands_wrapper));
	chatHandler->funcMap.insert(std::pair<std::string, std::function<void(rapidjson::Document&)>>("!class", class_wrapper));
	chatHandler->funcMap.insert(std::pair<std::string, std::function<void(rapidjson::Document&)>>("!selectClass", selectClass_wrapper));
	chatHandler->funcMap.insert(std::pair<std::string, std::function<void(rapidjson::Document&)>>("!job", job_wrapper));
	chatHandler->funcMap.insert(std::pair<std::string, std::function<void(rapidjson::Document&)>>("!listJobs", listJobs_wrapper));
	chatHandler->funcMap.insert(std::pair<std::string, std::function<void(rapidjson::Document&)>>("!selectJob", selectJob_wrapper));
	chatHandler->funcMap.insert(std::pair<std::string, std::function<void(rapidjson::Document&)>>("!listClasses", listClasses_wrapper));
	chatHandler->funcMap.insert(std::pair<std::string, std::function<void(rapidjson::Document&)>>("!listQuests", listQuests_wrapper));
	chatHandler->funcMap.insert(std::pair<std::string, std::function<void(rapidjson::Document&)>>("!describeQuest", describeQuest_wrapper));
	chatHandler->funcMap.insert(std::pair<std::string, std::function<void(rapidjson::Document&)>>("!startQuest", startQuest_wrapper));
	chatHandler->funcMap.insert(std::pair<std::string, std::function<void(rapidjson::Document&)>>("!joinQuest", joinQuest_wrapper));
}

void Bot::Init()
{
	saved_bot = this;
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

void Bot::verifyUser(std::string Name)
{
	if (!usersState.HasMember("Users"))
	{
		Value a(kArrayType);
		usersState.AddMember("Users", a, usersState.GetAllocator());
	}
	if (!arrayContains(usersState["Users"], Name))
	{
		Value userObject(kObjectType);
		userObject.AddMember("Name", Name, usersState.GetAllocator());
		
		Value stats(kObjectType);
		stats.AddMember("Strength", 0, usersState.GetAllocator());
		stats.AddMember("Dexterity", 0, usersState.GetAllocator());
		stats.AddMember("Constitution", 0, usersState.GetAllocator());
		stats.AddMember("Intelligence", 0, usersState.GetAllocator());
		stats.AddMember("Wisdom", 0, usersState.GetAllocator());
		stats.AddMember("Charisma", 0, usersState.GetAllocator());
		
		userObject.AddMember("Stats", stats, usersState.GetAllocator());

		userObject.AddMember("BaseClass", "No Class", usersState.GetAllocator());
		userObject.AddMember("ClassCurrentXP", 0, usersState.GetAllocator());
		userObject.AddMember("ClassTotalXP", 0, usersState.GetAllocator());
		userObject.AddMember("ClassLevel", 0, usersState.GetAllocator());
		
		userObject.AddMember("BaseJob", "No Job", usersState.GetAllocator());
		userObject.AddMember("JobCurrentXP", 0, usersState.GetAllocator());
		userObject.AddMember("JobTotalXP", 0, usersState.GetAllocator());
		userObject.AddMember("JobLevel", 0, usersState.GetAllocator());
		
		userObject.AddMember("ViewerLevel", 0, usersState.GetAllocator());
		userObject.AddMember("ViewerCurrentXP", 0, usersState.GetAllocator());
		userObject.AddMember("ViewerTotalXP", 0, usersState.GetAllocator());
		
		userObject.AddMember("Currency", 0, usersState.GetAllocator());
		
		userObject.AddMember("Location", "Guild Town", usersState.GetAllocator());
		
		userObject.AddMember("Energy", 5, usersState.GetAllocator());
		
		usersState["Users"].PushBack(userObject, usersState.GetAllocator());
	}
}

std::map<std::string, int> Bot::getViewerStats(std::string Name)
{
	verifyUser(Name);
	std::map<std::string, int> stats;
	for (auto& v : usersState["Users"].GetArray())
	{
		if (v["Name"].GetString() == Name)
		{
			for (rapidjson::Value::ConstMemberIterator iter = v["Stats"].MemberBegin(); iter != v["Stats"].MemberEnd(); ++iter)
			{
				stats[iter->name.GetString()] = iter->value.GetInt();
			}
		}
	}

	return stats;
}

int Bot::getViewerCurrentXp(std::string Name)
{
	verifyUser(Name);
	for (auto& v : usersState["Users"].GetArray())
	{
		if (v["Name"].GetString() == Name)
		{
			return v["ViewerCurrentXP"].GetInt();
		}
	}

	return -1;
}

int Bot::getLevel(std::string Name)
{
	verifyUser(Name);
	for (auto& v : usersState["Users"].GetArray())
	{
		if (v["Name"].GetString() == Name)
		{
			return v["ViewerLevel"].GetInt();
		}
	}

	return -1;
}

std::string Bot::getClass(std::string Name)
{
	verifyUser(Name);
	for (auto& v : usersState["Users"].GetArray())
	{
		if (v["Name"].GetString() == Name)
		{
			return v["BaseClass"].GetString();
		}
	}

	return "No Class";
}

std::string Bot::getJob(std::string Name)
{
	verifyUser(Name);
	for (auto& v : usersState["Users"].GetArray())
	{
		if (v["Name"].GetString() == Name)
		{
			return v["BaseJob"].GetString();
		}
	}

	return "No Job";
}

void Bot::incrementXp(std::string Name, int xpGain)
{
	verifyUser(Name);
	for (auto& v : usersState["Users"].GetArray())
	{
		if (v["Name"].GetString() == Name)
		{
			v["ViewerCurrentXP"] = v["ViewerCurrentXP"].GetInt() + xpGain;
			return;
		}
	}
}

bool Bot::RunStep(Quests::QuestStepPtr questStep, map<string, int> totalUserStats, int step)
{
	map<string, int> questRequirments;
	for (std::map<string, int>::iterator it = questStep->eventList[step]->Requirments.begin(); it != questStep->eventList[step]->Requirments.end(); ++it)
	{
		if (questRequirments.count(it->first) == 0)
		{
			questRequirments[it->first] = 0;
		}
		questRequirments[it->first] = questRequirments[it->first] + it->second;
	}

	bool met = true;

	for (std::map<string, int>::iterator it = questRequirments.begin(); it != questRequirments.end(); ++it)
	{
		if (totalUserStats.count(it->first) == 0 || totalUserStats[it->first] < it->second)
		{
			met = false;
		}
	}
	chatHandler->sendMessage(questStep->eventList[step]->Text);
	std::this_thread::sleep_for(std::chrono::seconds(5));
	int chance = rand() % 100 + 1;


	if ((met && chance <= questStep->eventList[step]->SuccessRateIfMet) || (!met && chance <= questStep->eventList[step]->SuccessRateIfNotMet))
	{
		chatHandler->sendMessage(questStep->eventList[step]->SuccessText);
		return true;
	}
	else
	{
		chatHandler->sendMessage(questStep->eventList[step]->FailText);
		return false;
	}
}

void Bot::RunQuest(Quests::QuestPtr quest)
{
	std::this_thread::sleep_for(std::chrono::seconds(30));
	chatHandler->sendMessage("Starting quest. When last we left our adventures");
	chatHandler->sendMessage(quest->Description);
	questManager->questStarted = true;

	bool questFailed = false;

	map<string, int> totalUserStats;

	for (auto& adventurer : questManager->adventurers)
	{
		map<string, int> userStats = getViewerStats(adventurer);
		for (std::map<string, int>::iterator it = userStats.begin(); it != userStats.end(); ++it)
		{
			if (totalUserStats.count(it->first) == 0)
			{
				totalUserStats[it->first] = 0;
			}
			totalUserStats[it->first] = totalUserStats[it->first] + it->second;
		}
	}

	for (int i = 0; i < quest->questStepList.size(); i++)
	{
		Quests::QuestStepPtr questStep = quest->questStepList[i];
		if (!questFailed)
		{
			if (questStep->RandomEvents == 0)
			{
				bool stepResult = RunStep(questStep, totalUserStats, 0);
				if (!stepResult)
				{
					questFailed = true;
				}
			}
			else
			{
				int stepsPassed = 0;
				for (int j = 0; j < questStep->RandomEvents && stepsPassed != questStep->ToSucced; j++)
				{
					int step = rand() % questStep->eventList.size();
					bool stepResult = RunStep(questStep, totalUserStats, step);
					if (stepResult)
					{
						stepsPassed++;
					}
				}
				if (stepsPassed != questStep->ToSucced)
				{
					questFailed = true;
				}
			}
		}
	}

	if (questFailed)
	{
		chatHandler->sendMessage(quest->FailText);
	}
	else
	{
		chatHandler->sendMessage(quest->SuccessText);
	}

	questManager->isQuestActive = false;
	questManager->questStarted = false;
	questManager->adventurers.empty();

}

void Bot::StartQuest(Quests::QuestPtr quest, string startingUser)
{
	questManager->activeQuest = quest;

	questManager->isQuestActive = true;
	questManager->questStarted = false;
	questManager->currentAdventurers = 1;
	questManager->maxAdventurers = quest->Slots;

	questManager->adventurers.empty();
	questManager->adventurers.emplace_back(startingUser);

	thread questRunner(&Bot::RunQuest, this, quest);
	questRunner.detach();
}

int Bot::chat_run(unsigned int maxEventsToProcess)
{
	if (0 == maxEventsToProcess)
	{
		return MIXER_OK;
	}

	if (chatHandler->shutdownRequested)
	{
		return MIXER_ERROR_CANCELLED;
	}

	// Create a local queue and populate it with the top elements from the incoming event queue to minimize locking time.
	chat_event_queue processingQueue;
	{
		std::lock_guard<std::mutex> incomingLock(chatHandler->incomingMutex);
		for (unsigned int i = 0; i < maxEventsToProcess && i < chatHandler->incomingEvents.size(); ++i)
		{
			processingQueue.emplace(std::move(chatHandler->incomingEvents.top()));
			chatHandler->incomingEvents.pop();
		}
	}

	// Process all the events in the local queue.
	while (!processingQueue.empty())
	{
		auto ev = processingQueue.top();
		switch (ev->type)
		{
		case chat_event_type_error:
		{
			DnDPanel::Logger::Info("chat_event_type_error");
			auto errorEvent = reinterpret_cast<std::shared_ptr<error_event>&>(ev);
			if (chatHandler->onError)
			{
				chatHandler->onError(chatHandler->callerContext, errorEvent->error.first, errorEvent->error.second.c_str(), errorEvent->error.second.length());
				if (chatHandler->shutdownRequested)
				{
					return MIXER_OK;
				}
			}
			break;
		}
		case chat_event_type_state_change:
		{
			DnDPanel::Logger::Info("chat_event_type_state_change");
			auto stateChangeEvent = reinterpret_cast<std::shared_ptr<state_change_event>&>(ev);
			chat_state previousState = chatHandler->state;
			chatHandler->state = stateChangeEvent->currentState;
			if (chatHandler->onStateChanged)
			{
				chatHandler->onStateChanged(chatHandler->callerContext, previousState, chatHandler->state);
			}
			break;
		}
		case chat_event_type_rpc_reply:
		{
			/*
			DnDPanel::Logger::Info("chat_event_type_rpc_reply");
			auto replyEvent = reinterpret_cast<std::shared_ptr<rpc_reply_event>&>(ev);
			replyEvent->replyHandler(chatBot, *replyEvent->replyJson);
			*/
			break;
		}
		case chat_event_type_http_response:
		{
			DnDPanel::Logger::Info("chat_event_type_http_response");
			auto httpResponseEvent = reinterpret_cast<std::shared_ptr<http_response_event>&>(ev);
			httpResponseEvent->responseHandler(httpResponseEvent->response);
			break;
		}
		case chat_event_type_rpc_method:
		{
			DnDPanel::Logger::Info("chat_event_type_rpc_method");
			auto rpcMethodEvent = reinterpret_cast<std::shared_ptr<rpc_method_event>&>(ev);
			if (rpcMethodEvent->methodJson->HasMember(RPC_SEQUENCE))
			{
				chatHandler->sequenceId = (*rpcMethodEvent->methodJson)[RPC_SEQUENCE].GetInt();
			}

			RETURN_IF_FAILED(route_method(*rpcMethodEvent->methodJson));
			break;
		}
		case chat_event_type_rpc_event:
		{
			auto rpcMethodEvent = reinterpret_cast<std::shared_ptr<rpc_method_event>&>(ev);
			if (rpcMethodEvent->methodJson->HasMember(RPC_SEQUENCE))
			{
				chatHandler->sequenceId = (*rpcMethodEvent->methodJson)[RPC_SEQUENCE].GetInt();
			}

			RETURN_IF_FAILED(route_event(*rpcMethodEvent->methodJson));
			break;
		}
		default:
			DnDPanel::Logger::Info("default");
			break;
		}

		processingQueue.pop();
		if (chatHandler->shutdownRequested)
		{
			return MIXER_OK;
		}
	}

	return MIXER_OK;
}

int Bot::route_method(rapidjson::Document& doc)
{
	std::string method = doc[RPC_METHOD].GetString();
	auto itr = chatHandler->methodHandlers.find(method);
	if (itr != chatHandler->methodHandlers.end())
	{
		return itr->second(chatHandler, doc);
	}
	else
	{
		DnDPanel::Logger::Info("Unhandled method type: " + method);
		if (chatHandler->onUnhandledMethod)
		{
			std::string methodJson = mixer_internal::jsonStringify(doc);
			chatHandler->onUnhandledMethod(chatHandler->callerContext, methodJson.c_str(), methodJson.length());
		}
	}

	return MIXER_OK;
}



int Bot::route_event(rapidjson::Document& doc)
{
	std::string method = doc[RPC_EVENT].GetString();
	auto itr = chatHandler->methodHandlers.find(method);
	if (itr != chatHandler->methodHandlers.end())
	{
		return itr->second(chatHandler, doc);
	}
	else
	{
		DnDPanel::Logger::Info("Unhandled method type: " + method);
		if (chatHandler->onUnhandledMethod)
		{
			std::string methodJson = mixer_internal::jsonStringify(doc);
			chatHandler->onUnhandledMethod(chatHandler->callerContext, methodJson.c_str(), methodJson.length());
		}
	}

	return MIXER_OK;
}