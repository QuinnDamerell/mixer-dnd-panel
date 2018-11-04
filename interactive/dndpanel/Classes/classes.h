#pragma once

#include "../dndpanel/common.h"

#include <string>
#include <vector>
#include <map>

#include "rapidjson/filereadstream.h"
#include "rapidjson/document.h"

namespace Classes
{
	DECLARE_SMARTPOINTER(classInfo);
	class classInfo :
		public SharedFromThis
	{
	public:
		classInfo(std::string baseName, std::string mainStat, std::map<std::string, int> levelToXp, std::map<std::string, std::string> levelToName)
		{
			BaseName = baseName;
			MainStat = mainStat;

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

		std::string BaseName;
		std::string MainStat;
		std::map<int, int> LevelToRequiredXp;
		std::map<int, std::string> LevelToName;

	};

	DECLARE_SMARTPOINTER(classInfoList);
	class classInfoList :
		public SharedFromThis
	{
	public:
		classInfoList(std::string filelocation)
		{
			FILE* fp = fopen(filelocation.c_str(), "rb"); // non-Windows use "r"

			if (fp == nullptr)
			{
				printf("Class list was unable to open. There will be no classes\n");

				return;
			}


			char readBuffer[65536];
			rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
			rapidjson::Document jobState;
			jobState.ParseStream(is);

			fclose(fp);

			if (jobState.IsNull())
			{
				printf("Class list was unable to parse. There will be no classes\n");

				return;
			}

			for (auto& v : jobState["jobs"].GetArray())
			{
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

				classInfoPtr tempJob = std::make_shared<classInfo>(
					v["BaseName"].GetString(), 
					v["MainStat"].GetString(), 
					tempLevelToXp,
					tempLevelToName);

				classInfos.push_back(tempJob);
			}
		}

		std::vector<classInfoPtr> classInfos;
	};
}