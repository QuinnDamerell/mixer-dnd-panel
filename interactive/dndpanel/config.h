#pragma once

#include "common.h"
#include <string>
#include <map>

#define CLIENT_ID		"83068dc323fd2e16eddfe89970e548a277c46e047a08d0dd"
#define INTERACTIVE_ID	"282246"
#define SHARE_CODE		"kfjawxep"

namespace DnDPanel
{
    DECLARE_SMARTPOINTER(DndConfig);
    class DndConfig
    {
    public:
        int Init(std::string);
        int Write();

        std::string ClientId = CLIENT_ID;
        std::string InteractiveId = INTERACTIVE_ID;
        std::string ShareCode = SHARE_CODE;
        std::string RefreshToken;
		std::string configFileLocation;
    };

	DECLARE_SMARTPOINTER(ChatConfig);
	class ChatConfig
	{
	public:
		int Init(std::string);
		int Write();

		std::string ClientId = CLIENT_ID;
        std::string InteractiveId = INTERACTIVE_ID;
        std::string ShareCode = SHARE_CODE;
		std::string RefreshToken;
		std::string configFileLocation;
		std::map<std::string, int> levels;
	};
}