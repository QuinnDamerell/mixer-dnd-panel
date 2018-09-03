#pragma once

#include "common.h"

#define CLIENT_ID		"83068dc323fd2e16eddfe89970e548a277c46e047a08d0dd"
#define INTERACTIVE_ID	"282246"
#define SHARE_CODE		"kfjawxep"

namespace DnDPanel
{
    DECLARE_SMARTPOINTER(DndConfig);
    class DndConfig
    {
    public:
        int Init();
        int Write();

        std::string ClientId = CLIENT_ID;
        std::string InteractiveId = INTERACTIVE_ID;
        std::string ShareCode = SHARE_CODE;
        std::string RefreshToken;
    };
}