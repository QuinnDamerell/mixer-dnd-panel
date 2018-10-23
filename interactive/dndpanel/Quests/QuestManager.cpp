#include "QuestManager.h"

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"

#include <experimental/filesystem>

#include <stdio.h>
#include <fstream>
#include <iostream>
#include <algorithm>

using namespace Quests;
using namespace std;
using namespace std::experimental::filesystem;

QuestManager::QuestManager()
{
	cout << "Starting Constructor" << endl;
	for (auto& dirEntry : recursive_directory_iterator("dndpanel\\Quests\\Recipies"))
	{
		std::stringstream buffer;
		buffer << dirEntry << endl;
		std::string filePath = buffer.str();
		std::replace(filePath.begin(), filePath.end(), '\\', '/');
		filePath.erase(std::remove(filePath.begin(), filePath.end(), '\n'), filePath.end());
		cout << filePath << endl;
		cout << "dndpanel/Quests/Recipies/castleraven.json" << endl;
		FILE* fp = fopen(filePath.c_str(), "rb"); // non-Windows use "r"
		rapidjson::Document contents;


		char readBuffer[65536];
		rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
		contents.ParseStream(is);
		fclose(fp);

		cout << contents["Name"].GetString() << endl;
	}
}