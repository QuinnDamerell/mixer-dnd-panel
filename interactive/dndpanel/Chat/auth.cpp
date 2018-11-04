#include "ChatHandler.h"
#include "internal/interactive_session.h"

#include <windows.h>
#undef GetObject

#define JSON_CODE "code"
#define JSON_HANDLE "handle"

using namespace Chat;

std::string Auth::getAuthToken()
{
	return authToken;
}

int Auth::chat_auth_get_short_code(const char* clientId, const char* clientSecret, char* shortCode, size_t* shortCodeLength, char* shortCodeHandle, size_t* shortCodeHandleLength)
{
	if (nullptr == clientId || nullptr == shortCode || nullptr == shortCodeLength || nullptr == shortCodeHandle || nullptr == shortCodeHandleLength)
	{
		return MIXER_ERROR_INVALID_POINTER;
	}

	mixer_internal::http_response response;
	std::string oauthCodeUrl = "https://mixer.com/api/v1/oauth/shortcode";

	// Construct the json body
	std::string jsonBody;
	if (nullptr == clientSecret)
	{
		jsonBody = std::string("{ \"client_id\": \"") + clientId + "\", \"scope\": \"chat:chat chat:connect chat:whisper chat:remove_message\" }";
	}
	else
	{
		jsonBody = std::string("{ \"client_id\": \"") + clientId + "\", \"client_secret\": \"" + clientSecret + "\", \"scope\": \"interactive:robot:self\" }";
	}

	std::unique_ptr<mixer_internal::http_client> client = mixer_internal::http_factory::make_http_client();
	RETURN_IF_FAILED(client->make_request(oauthCodeUrl, "POST", nullptr, jsonBody, response));
	if (200 != response.statusCode)
	{
		return response.statusCode;
	}

	rapidjson::Document doc;
	if (doc.Parse(response.body.c_str()).HasParseError())
	{
		return MIXER_ERROR_JSON_PARSE;
	}

	std::string code = doc[JSON_CODE].GetString();
	std::string handle = doc[JSON_HANDLE].GetString();

	if (*shortCodeLength < code.length() + 1 ||
		*shortCodeHandleLength < handle.length() + 1)
	{
		*shortCodeLength = code.length() + 1;
		*shortCodeHandleLength = handle.length() + 1;
		return MIXER_ERROR_BUFFER_SIZE;
	}

	memcpy(shortCode, code.c_str(), code.length());
	shortCode[code.length()] = 0;
	*shortCodeLength = code.length() + 1;

	memcpy(shortCodeHandle, handle.c_str(), handle.length());
	shortCodeHandle[handle.length()] = 0;
	*shortCodeHandleLength = handle.length() + 1;
	return MIXER_OK;
}

int Auth::EnsureAuth(DnDPanel::DndConfigPtr config)
{
	int err = 0;

	this->authToken = std::string("");

	// Try to use the old token if it exists.
	do
	{
		// If we have a token, try to refresh.
		if (config->RefreshToken.length() > 0)
		{
			char newToken[1024];
			size_t newTokenLength = _countof(newToken);
			err = interactive_auth_refresh_token(config->ClientId.c_str(), nullptr, config->RefreshToken.c_str(), newToken, &newTokenLength);
			if (!err)
			{
				config->RefreshToken = std::string(newToken, newTokenLength);
				break;
			}
			else
			{
				// If failed clear the token
				config->RefreshToken = "";
			}
		}
		else
		{
			// Start a clean auth
			char shortCode[7];
			size_t shortCodeLength = _countof(shortCode);
			char shortCodeHandle[1024];
			size_t shortCodeHandleLength = _countof(shortCodeHandle);
			err = interactive_auth_get_short_code(CLIENT_ID, nullptr, shortCode, &shortCodeLength, shortCodeHandle, &shortCodeHandleLength);

			if (err) return err;

			// Pop the browser for the user to approve access.
			std::string authUrl = std::string("https://www.mixer.com/go?code=") + shortCode;
			ShellExecuteA(0, 0, authUrl.c_str(), nullptr, nullptr, SW_SHOW);

			// Wait for OAuth token response.
			char refreshTokenBuffer[1024];
			size_t refreshTokenLength = _countof(refreshTokenBuffer);
			err = interactive_auth_wait_short_code(CLIENT_ID, nullptr, shortCodeHandle, refreshTokenBuffer, &refreshTokenLength);
			if (err)
			{
				if (MIXER_ERROR_TIMED_OUT == err)
				{
					std::cout << "Authorization timed out, user did not approve access within the time limit." << std::endl;
				}
				else if (MIXER_ERROR_AUTH_DENIED == err)
				{
					std::cout << "User denied access." << std::endl;
				}

				return err;
			}

			// Cache the refresh token
			config->RefreshToken = std::string(refreshTokenBuffer, refreshTokenLength);
			break;
		}
	} while (config->RefreshToken.length() == 0);

	// Write the config.
	config->Write();

	// Extract the authorization header from the refresh token.
	char authBuffer[1024];
	size_t authBufferLength = _countof(authBuffer);
	err = interactive_auth_parse_refresh_token(config->RefreshToken.c_str(), authBuffer, &authBufferLength);
	if (err)
	{
		return err;
	}

	// Success!
	authToken = std::string(authBuffer, authBufferLength);
	return 0;
}

int Auth::EnsureAuth(DnDPanel::ChatConfigPtr config)
{
	int err = 0;

	this->authToken = std::string("");

	// Try to use the old token if it exists.
	do
	{
		// If we have a token, try to refresh.
		if (config->RefreshToken.length() > 0)
		{
			char newToken[1024];
			size_t newTokenLength = _countof(newToken);
			err = interactive_auth_refresh_token(config->ClientId.c_str(), nullptr, config->RefreshToken.c_str(), newToken, &newTokenLength);
			if (!err)
			{
				config->RefreshToken = std::string(newToken, newTokenLength);
				break;
			}
			else
			{
				// If failed clear the token
				config->RefreshToken = "";
			}
		}
		else
		{
			// Start a clean auth
			char shortCode[7];
			size_t shortCodeLength = _countof(shortCode);
			char shortCodeHandle[1024];
			size_t shortCodeHandleLength = _countof(shortCodeHandle);
			err = chat_auth_get_short_code(CLIENT_ID, nullptr, shortCode, &shortCodeLength, shortCodeHandle, &shortCodeHandleLength);

			if (err) return err;

			// Pop the browser for the user to approve access.
			std::string authUrl = std::string("https://www.mixer.com/go?code=") + shortCode;
			ShellExecuteA(0, 0, authUrl.c_str(), nullptr, nullptr, SW_SHOW);

			// Wait for OAuth token response.
			char refreshTokenBuffer[1024];
			size_t refreshTokenLength = _countof(refreshTokenBuffer);
			err = interactive_auth_wait_short_code(CLIENT_ID, nullptr, shortCodeHandle, refreshTokenBuffer, &refreshTokenLength);
			if (err)
			{
				if (MIXER_ERROR_TIMED_OUT == err)
				{
					std::cout << "Authorization timed out, user did not approve access within the time limit." << std::endl;
				}
				else if (MIXER_ERROR_AUTH_DENIED == err)
				{
					std::cout << "User denied access." << std::endl;
				}

				return err;
			}

			// Cache the refresh token
			config->RefreshToken = std::string(refreshTokenBuffer, refreshTokenLength);
			break;
		}
	} while (config->RefreshToken.length() == 0);

	// Write the config.
	config->Write();

	// Extract the authorization header from the refresh token.
	char authBuffer[1024];
	size_t authBufferLength = _countof(authBuffer);
	err = interactive_auth_parse_refresh_token(config->RefreshToken.c_str(), authBuffer, &authBufferLength);
	if (err)
	{
		return err;
	}

	// Success!
	authToken = std::string(authBuffer, authBufferLength);
	return 0;
}