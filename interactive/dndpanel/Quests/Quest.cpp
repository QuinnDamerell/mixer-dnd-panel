#include "Quest.h"

using namespace Quests;
using namespace std;
using namespace rapidjson;

Event::Event(rapidjson::Value& recipe)
{
	Text = recipe["Text"].GetString();
	SuccessText = recipe["SuccessText"].GetString();
	FailText = recipe["FailText"].GetString();
	SuccessRateIfMet = recipe["SuccessRateIfMet"].GetInt();
	SuccessRateIfNotMet = recipe["SuccessRateIfNotMet"].GetInt();
	
	for (rapidjson::Value::ConstMemberIterator iter = recipe["Requirements"].MemberBegin(); iter != recipe["Requirements"].MemberEnd(); ++iter)
	{
		Requirments[iter->name.GetString()] = iter->value.GetInt();
	}
}

QuestStep::QuestStep(Value& recipe)
{
	StepNumber = recipe["StepNumber"].GetInt();
	RandomEvents = recipe["RandomEvents"].GetInt();
	ToSucced = recipe["ToSucced"].GetInt();

	for (auto& v : recipe["Events"].GetArray())
	{
		EventPtr eventInfo = make_shared<Event>(v);
		eventList.emplace_back(eventInfo);
	}
}

Quest::Quest(Document& recipe)
{
	Name = recipe["Name"].GetString();
	Description = recipe["Description"].GetString();
	SuccessText = recipe["SuccessText"].GetString();
	FailText = recipe["FailText"].GetString();
	Slots = recipe["Slots"].GetInt();

	for (auto& v : recipe["Steps"].GetArray())
	{
		QuestStepPtr questStep = make_shared<QuestStep>(v);
		questStepList.push_back(questStep);
	}
}