#include "QuestManager.h"

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"

#include <experimental/filesystem>

#include <stdio.h>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <thread>
#include <stdlib.h>

using namespace Quests;
using namespace std;
using namespace std::experimental::filesystem;
using namespace rapidjson;

QuestManager::QuestManager()
{
	isQuestActive = false;
	srand(time(NULL));

	for (auto& dirEntry : recursive_directory_iterator("dndpanel\\Quests\\Recipies"))
	{
		std::stringstream buffer;
		buffer << dirEntry << endl;
		string filePath = buffer.str();
		replace(filePath.begin(), filePath.end(), '\\', '/');
		filePath.erase(remove(filePath.begin(), filePath.end(), '\n'), filePath.end());
		FILE* fp = fopen(filePath.c_str(), "rb"); // non-Windows use "r"
		Document contents;


		char readBuffer[65536];
		FileReadStream is(fp, readBuffer, sizeof(readBuffer));
		contents.ParseStream(is);
		fclose(fp);

		QuestPtr temp = make_shared<Quest>(contents);
		QuestList.emplace_back(temp);

		
	}
}