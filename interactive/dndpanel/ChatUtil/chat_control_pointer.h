#include <string>

namespace ChatUtil
{
	struct chat_control_pointer
	{
		const std::string sceneId;
		const std::string cachePointer;
		chat_control_pointer(std::string sceneId, std::string cachePointer) : sceneId(std::move(sceneId)), cachePointer(std::move(cachePointer)) {}
	};
}
