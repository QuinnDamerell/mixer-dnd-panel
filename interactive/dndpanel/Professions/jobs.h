#pragma once

#include "../common.h"

#include <string>
#include <vector>
#include <map>

#include "rapidjson/filereadstream.h"
#include "rapidjson/document.h"

namespace Professions
{
	DECLARE_SMARTPOINTER(job);
	class job :
		public SharedFromThis
	{
	public:
		job(std::string baseName, std::string knowledge, std::string resource, std::string workText, std::map<std::string,int> income, std::map<std::string, int> levelToXp, std::map<std::string, std::string> levelToName)
		{
			BaseName = baseName;
			Knowledge = knowledge;
			Resource = resource;
			WorkText = workText;

			for (const auto& pair : income)
			{
				Income.emplace(atoi(pair.first.c_str()), pair.second);
			}

			for (const auto& pair : levelToXp)
			{
				LevelToRequiredXp.emplace(atoi(pair.first.c_str()), pair.second);
			}

			for (const auto& pair : levelToName)
			{
				LevelToName.emplace(atoi(pair.first.c_str()), pair.second);
			}			
		}

		int GetLevel(int jobXp)
		{
			for (const auto& pair : LevelToRequiredXp)
			{
				if (jobXp < pair.second)
				{
					return pair.first;
				}
			}

		}

		std::string GetLevelName(int jobLevel)
		{
			return LevelToName[jobLevel];
		}

		int GetIncome(int jobLevel)
		{
			return Income[jobLevel];
		}

		std::string BaseName;
		std::string Knowledge;
		std::string Resource;
		std::string WorkText;
		std::map<int, int> Income;
		std::map<int, int> LevelToRequiredXp;
		std::map<int, std::string> LevelToName;

	};

	DECLARE_SMARTPOINTER(jobslist);
	class jobslist :
		public SharedFromThis
	{
	public:
		jobslist(std::string filelocation)
		{
			FILE* fp = fopen(filelocation.c_str(), "rb"); // non-Windows use "r"

			if (fp == nullptr)
			{
				printf("Job list was unable to open. There will be no jobs\n");

				return;
			}


			char readBuffer[65536];
			rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
			rapidjson::Document jobState;
			jobState.ParseStream(is);

			fclose(fp);

			if (jobState.IsNull())
			{
				printf("Job list was unable to open. There will be no jobs\n");

				return;
			}

			for (auto& v : jobState["jobs"].GetArray())
			{
				std::map<std::string, int> tempIncome;

				for (rapidjson::Value::ConstMemberIterator iter = v["Income"].MemberBegin(); iter != v["Income"].MemberEnd(); ++iter) 
				{
					tempIncome[iter->name.GetString()] = iter->value.GetInt();
				}

				std::map<std::string, int> tempLevelToXp;

				for (rapidjson::Value::ConstMemberIterator iter = v["LevelToRequiredXp"].MemberBegin(); iter != v["LevelToRequiredXp"].MemberEnd(); ++iter)
				{
					tempLevelToXp[iter->name.GetString()] = iter->value.GetInt();
				}

				std::map<std::string, std::string> tempLevelToName;

				for (rapidjson::Value::ConstMemberIterator iter = v["LevelToName"].MemberBegin(); iter != v["LevelToName"].MemberEnd(); ++iter)
				{
					tempLevelToName[iter->name.GetString()] = iter->value.GetString();
				}

				jobPtr tempJob = std::make_shared<job>(
					v["BaseName"].GetString(), 
					v["Knowledge"].GetString(), 
					v["Resource"].GetString(), 
					v["WorkText"].GetString(), 
					tempIncome, 
					tempLevelToXp,
					tempLevelToName);

				jobs.push_back(tempJob);
			}
		}

		std::vector<jobPtr> jobs;
	};
}