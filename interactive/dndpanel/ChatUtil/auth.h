#pragma once

#include "..\common.h"
#include "..\config.h"

namespace ChatUtil
{
	DECLARE_SMARTPOINTER(Auth);
	class Auth :
		public SharedFromThis
	{
	public:
		int EnsureAuth(DnDPanel::DndConfigPtr);
		int EnsureAuth(DnDPanel::ChatConfigPtr);
		std::string getAuthToken();

	private:
		int chat_auth_get_short_code(const char* clientId, const char* clientSecret, char* shortCode, size_t* shortCodeLength, char* shortCodeHandle, size_t* shortCodeHandleLength);
		std::string authToken;
	};
}
