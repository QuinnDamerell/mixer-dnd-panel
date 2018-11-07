#include "chat_session_internal.h"

using namespace ChatSession;
using namespace Chat;

chat_session_internal::chat_session_internal()
	: isReady(false),
	onInput(nullptr), onError(nullptr),
	onUnhandledMethod(nullptr), onControlChanged(nullptr), onTransactionComplete(nullptr), serverTimeOffsetMs(0),  scenesCached(false), groupsCached(false),
	getTimeRequestId(0xffffffff)
{
	scenesRoot.SetObject();
}





