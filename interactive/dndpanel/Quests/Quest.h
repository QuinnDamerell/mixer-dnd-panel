#pragma once

#include "..\dndpanel\common.h"
#include <vector>
#include "rapidjson/document.h"
#include <map>

namespace Quests
{
	DECLARE_SMARTPOINTER(Event);
	class Event :
		public SharedFromThis
	{
	public:
		Event(rapidjson::Value&);
		std::string Text;
		std::string SuccessText;
		std::string FailText;
		int SuccessRateIfMet;
		int SuccessRateIfNotMet;
		std::map<std::string, int> Requirments;
	};

	DECLARE_SMARTPOINTER(QuestStep);
	class QuestStep :
		public SharedFromThis
	{
	public:
		QuestStep(rapidjson::Value&);
		int StepNumber;
		int RandomEvents;
		int ToSucced;
		std::vector<EventPtr> eventList;
	};

	DECLARE_SMARTPOINTER(Quest);
	class Quest :
		public SharedFromThis
	{
	public:
		Quest(rapidjson::Document&);
		std::string Name;
		std::string Description;
		std::string SuccessText;
		std::string FailText;
		int Slots;
		std::vector<QuestStepPtr> questStepList;		
	};
}