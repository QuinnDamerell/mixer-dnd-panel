#include "chatbot.h"
#include "rapidjson/document.h"
#include <string>
#include "logger.h"
#include <functional>
#include "chatutil.h"

using namespace std;
using namespace RAPIDJSON_NAMESPACE;
using namespace Chat;
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
	sendWhisper(session, username + " has: " + to_string(getViewerCurrentXp(session, username)) + " xp.", username);
	deleteMessage(session, doc["data"]["id"].GetString());
}

void xp_wrapper(chat_session_internal& session, rapidjson::Document& doc)
{
	saved_bot->xp(session, doc);
}

void Bot::level(chat_session_internal& session, rapidjson::Document& doc)
{
	std::string username = doc["data"]["user_name"].GetString();
	sendWhisper(session, username + " is level: " + to_string(getLevel(session, username)) + ".", username);
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
		else
		{
			result += key;
			isFirst = false;
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

void Bot::cclass(chat_session_internal& session, rapidjson::Document& doc)
{
	std::string username = doc["data"]["user_name"].GetString();
	sendWhisper(session, username + " is class: " + getClass(session, username) + ".", username);
	deleteMessage(session, doc["data"]["id"].GetString());
}

void class_wrapper(chat_session_internal& session, rapidjson::Document& doc)
{
	saved_bot->cclass(session, doc);
}

void Bot::job(chat_session_internal& session, rapidjson::Document& doc)
{
	std::string username = doc["data"]["user_name"].GetString();
	sendWhisper(session, username + " is job: " + getJob(session, username) + ".", username);
	deleteMessage(session, doc["data"]["id"].GetString());
}

void job_wrapper(chat_session_internal& session, rapidjson::Document& doc)
{
	saved_bot->job(session, doc);
}

void Bot::listJobs(chat_session_internal& session, rapidjson::Document& doc)
{
	std::string result = "";

	bool isFirst = true;
	for (auto &job : session.jobList->jobs) // access by reference to avoid copying
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
	sendWhisper(session, username + " all jobs are: " + result + ".", username);
	deleteMessage(session, doc["data"]["id"].GetString());
}

void listJobs_wrapper(chat_session_internal& session, rapidjson::Document& doc)
{
	saved_bot->listJobs(session, doc);
}

void Bot::listClasses(ChatSession::chat_session_internal& session, rapidjson::Document& doc)
{
	std::string result = "";

	bool isFirst = true;
	for (auto &classInfo : session.classList->classInfos) // access by reference to avoid copying
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
	sendWhisper(session, username + " all classes are: " + result + ".", username);
	deleteMessage(session, doc["data"]["id"].GetString());
}

void listClasses_wrapper(chat_session_internal& session, rapidjson::Document& doc)
{
	saved_bot->listClasses(session, doc);
}

void Bot::selectJob(ChatSession::chat_session_internal& session, rapidjson::Document& doc)
{
	std::string username = doc["data"]["user_name"].GetString();

	if (getJob(session, username) != "No Job")
	{
		sendWhisper(session, username + "You already have a job which is:" + getJob(session, username), username);
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
			for (auto &job : session.jobList->jobs) // access by reference to avoid copying
			{
				std::string key = job->BaseName;

				if (key == selectedJob)
				{
					for (auto& v : session.usersState["Users"].GetArray())
					{
						if (v["Name"].GetString() == username)
						{
							v["BaseJob"].SetString(StringRef(key.c_str()), doc.GetAllocator());
							sendWhisper(session, username + "Setting your job to:" + key, username);
							foundJob = true;
						}
					}
				}
			}
			if (!foundJob)
			{
				sendWhisper(session, username + "The job you selected does not exist. Check listJobs to see to find a job.", username);
			}

		}
	}
	
	deleteMessage(session, doc["data"]["id"].GetString());
}

void selectJob_wrapper(chat_session_internal& session, rapidjson::Document& doc)
{
	saved_bot->selectJob(session, doc);
}

void Bot::selectClass(ChatSession::chat_session_internal& session, rapidjson::Document& doc)
{
	std::string username = doc["data"]["user_name"].GetString();

	if (getClass(session, username) != "No Class")
	{
		sendWhisper(session, username + "You already have a class which is:" + getClass(session, username), username);
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
			for (auto &classInfo : session.classList->classInfos) // access by reference to avoid copying
			{
				std::string key = classInfo->BaseName;

				if (key == selectedClass)
				{
					for (auto& v : session.usersState["Users"].GetArray())
					{
						if (v["Name"].GetString() == username)
						{
							v["BaseClass"].SetString(StringRef(key.c_str()), doc.GetAllocator());
							sendWhisper(session, username + "Setting your class to:" + key, username);
							foundClass = true;
						}
					}
				}
			}
			if (!foundClass)
			{
				sendWhisper(session, username + "The class you selected does not exist. Check listClasss to see to find a class.", username);
			}

		}
	}

	deleteMessage(session, doc["data"]["id"].GetString());
}

void selectClass_wrapper(chat_session_internal& session, rapidjson::Document& doc)
{
	saved_bot->selectClass(session, doc);
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
	funcMap.insert(std::pair<std::string, std::function<void(chat_session_internal&, rapidjson::Document&)>>("!class", class_wrapper));
	funcMap.insert(std::pair<std::string, std::function<void(chat_session_internal&, rapidjson::Document&)>>("!selectClass", selectClass_wrapper));
	funcMap.insert(std::pair<std::string, std::function<void(chat_session_internal&, rapidjson::Document&)>>("!job", job_wrapper));
	funcMap.insert(std::pair<std::string, std::function<void(chat_session_internal&, rapidjson::Document&)>>("!listJobs", listJobs_wrapper));
	funcMap.insert(std::pair<std::string, std::function<void(chat_session_internal&, rapidjson::Document&)>>("!selectJob", selectJob_wrapper));
	funcMap.insert(std::pair<std::string, std::function<void(chat_session_internal&, rapidjson::Document&)>>("!listClasses", listClasses_wrapper));
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
		std::string command = found_text.substr(0, found_text.find(" "));

		map<std::string, std::function<void(chat_session_internal&, rapidjson::Document&)>>::iterator it = funcMap.find(command);
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
		
		o.AddMember("Strength", 0, session.usersState.GetAllocator());
		o.AddMember("Dexterity", 0, session.usersState.GetAllocator());
		o.AddMember("Constitution", 0, session.usersState.GetAllocator());
		o.AddMember("Intelligence", 0, session.usersState.GetAllocator());
		o.AddMember("Wisdom", 0, session.usersState.GetAllocator());
		o.AddMember("Charisma", 0, session.usersState.GetAllocator());
		
		o.AddMember("BaseClass", "No Class", session.usersState.GetAllocator());
		o.AddMember("ClassCurrentXP", 0, session.usersState.GetAllocator());
		o.AddMember("ClassTotalXP", 0, session.usersState.GetAllocator());
		o.AddMember("ClassLevel", 0, session.usersState.GetAllocator());
		
		o.AddMember("BaseJob", "No Job", session.usersState.GetAllocator());
		o.AddMember("JobCurrentXP", 0, session.usersState.GetAllocator());
		o.AddMember("JobTotalXP", 0, session.usersState.GetAllocator());
		o.AddMember("JobLevel", 0, session.usersState.GetAllocator());
		
		o.AddMember("ViewerLevel", 0, session.usersState.GetAllocator());
		o.AddMember("ViewerCurrentXP", 0, session.usersState.GetAllocator());
		o.AddMember("ViewerTotalXP", 0, session.usersState.GetAllocator());
		
		o.AddMember("Currency", 0, session.usersState.GetAllocator());
		
		o.AddMember("Location", "Guild Town", session.usersState.GetAllocator());
		
		o.AddMember("Energy", 5, session.usersState.GetAllocator());
		
		session.usersState["Users"].PushBack(o, session.usersState.GetAllocator());
	}
}

int Bot::getViewerCurrentXp(chat_session_internal& session, std::string Name)
{
	verifyUser(session, Name);
	for (auto& v : session.usersState["Users"].GetArray())
	{
		if (v["Name"].GetString() == Name)
		{
			return v["ViewerCurrentXP"].GetInt();
		}
	}

	return -1;
}

int Bot::getLevel(chat_session_internal& session, std::string Name)
{
	verifyUser(session, Name);
	for (auto& v : session.usersState["Users"].GetArray())
	{
		if (v["Name"].GetString() == Name)
		{
			return v["ViewerLevel"].GetInt();
		}
	}

	return -1;
}

std::string Bot::getClass(chat_session_internal& session, std::string Name)
{
	verifyUser(session, Name);
	for (auto& v : session.usersState["Users"].GetArray())
	{
		if (v["Name"].GetString() == Name)
		{
			return v["BaseClass"].GetString();
		}
	}

	return "No Class";
}

std::string Bot::getJob(chat_session_internal& session, std::string Name)
{
	verifyUser(session, Name);
	for (auto& v : session.usersState["Users"].GetArray())
	{
		if (v["Name"].GetString() == Name)
		{
			return v["BaseJob"].GetString();
		}
	}

	return "No Job";
}

void Bot::incrementXp(chat_session_internal& session, std::string Name, int xpGain)
{
	verifyUser(session, Name);
	for (auto& v : session.usersState["Users"].GetArray())
	{
		if (v["Name"].GetString() == Name)
		{
			v["ViewerCurrentXP"] = v["ViewerCurrentXP"].GetInt() + xpGain;
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