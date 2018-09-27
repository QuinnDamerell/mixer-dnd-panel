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

int DndConfig::Init(std::string configFileLocation)
{
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
    std::ofstream ofs("dndpanelconfig.json");
    OStreamWrapper osw(ofs);
    Writer<OStreamWrapper> writer(osw);
    d.Accept(writer);

    return 0;
}

