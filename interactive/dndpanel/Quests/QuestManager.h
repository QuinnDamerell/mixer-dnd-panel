#include "..\common.h"

namespace Quests
{
	DECLARE_SMARTPOINTER(QuestManager);
	class QuestManager :
		public SharedFromThis
	{
	public:
		QuestManager();
	};
}