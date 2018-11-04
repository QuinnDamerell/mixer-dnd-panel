#pragma once

#include "..\dndpanel\common.h"
#include "Quest.h"
#include <vector>

namespace Quests
{
	DECLARE_SMARTPOINTER(QuestManager);
	class QuestManager :
		public SharedFromThis
	{
	public:
		QuestManager();

		std::vector<QuestPtr> QuestList;

		QuestPtr activeQuest;
		bool isQuestActive;
		bool questStarted;
		int currentAdventurers;
		int maxAdventurers;
		std::vector<std::string> adventurers;
	};
}