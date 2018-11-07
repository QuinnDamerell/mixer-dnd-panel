#pragma once

#include "interactivity.h"

namespace Chat
{
	struct chat_participant : public interactive_object
	{
		unsigned int userId;
		const char* userName;
		size_t usernameLength;
		unsigned int level;
		unsigned long long lastInputAtMs;
		unsigned long long connectedAtMs;
		bool disabled;
		const char* groupId;
		size_t groupIdLength;
	};
}
