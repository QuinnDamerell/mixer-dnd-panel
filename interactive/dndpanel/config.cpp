#include "config.h"

#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/ostreamwrapper.h"
#include "rapidjson/writer.h"

#include <iostream>
#include <fstream>
#include <string>


using namespace DnDPanel;
using namespace rapidjson;

#define REFRESH_TOKEN_TOKEN "RefreshToken"

int DndConfig::Init(std::string configFileLoc)
{
	configFileLocation = configFileLoc;

    std::ifstream configFile;
    configFile.open(configFileLocation);
    if (!configFile.is_open())
    {
        Logger::Info("No config file found, creating a new config.");
        return 0;
    }

    IStreamWrapper isw(configFile);
    Document d;
    d.ParseStream(isw);

    if (!d.IsObject())
    {
        return 1;
    }

    // Grab the refresh token
    if (d.HasMember(REFRESH_TOKEN_TOKEN) && d[REFRESH_TOKEN_TOKEN].IsString())
    {
        RefreshToken = d[REFRESH_TOKEN_TOKEN].GetString();
    }

    return 0;
}

int DndConfig::Write()
{
    // Build the json.
    Document d;
    d.SetObject();
    {
        Value v;
        v.SetString(RefreshToken, d.GetAllocator());
        d.AddMember(REFRESH_TOKEN_TOKEN, v, d.GetAllocator());
    }

    // Write to the file.
    std::ofstream ofs(configFileLocation);
    OStreamWrapper osw(ofs);
    Writer<OStreamWrapper> writer(osw);
    d.Accept(writer);

    return 0;
}

int ChatConfig::Init(std::string configFileLoc)
{
	configFileLocation = configFileLoc;

	std::ifstream configFile;
	configFile.open(configFileLocation);
	if (!configFile.is_open())
	{
		Logger::Info("No chat config file found, creating a new chat config.");
		return 0;
	}

	IStreamWrapper isw(configFile);
	Document d;
	d.ParseStream(isw);

	if (!d.IsObject())
	{
		return 1;
	}

	// Grab the refresh token
	if (d.HasMember(REFRESH_TOKEN_TOKEN) && d[REFRESH_TOKEN_TOKEN].IsString())
	{
		RefreshToken = d[REFRESH_TOKEN_TOKEN].GetString();
	}

	std::ifstream levelFile;
	levelFile.open("config-files/levelxp.json");
	if (!levelFile.is_open())
	{
		Logger::Info("No level config file found, creating a new chat config.");
		return 0;
	}

	IStreamWrapper iswLevel(levelFile);
	Document dLevel;
	dLevel.ParseStream(iswLevel);

	if (!dLevel.IsObject())
	{
		return 1;
	}

	for (Value::ConstMemberIterator iter = dLevel.MemberBegin(); iter != dLevel.MemberEnd(); ++iter) 
	{
		levels[iter->name.GetString()] = iter->value.GetInt();
	}

	return 0;
}

int ChatConfig::Write()
{
	// Build the json.
	Document d;
	d.SetObject();
	{
		Value v;
		v.SetString(RefreshToken, d.GetAllocator());
		d.AddMember(REFRESH_TOKEN_TOKEN, v, d.GetAllocator());
	}

	// Write to the file.
	std::ofstream ofs(configFileLocation);
	OStreamWrapper osw(ofs);
	Writer<OStreamWrapper> writer(osw);
	d.Accept(writer);

	return 0;
}

