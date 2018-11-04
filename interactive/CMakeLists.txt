cmake_minimum_required (VERSION 2.8.0)
enable_language(C CXX)
project(MixerDndPanel)

# Use for debugging.
# set(CMAKE_BUILD_TYPE Debug)

# Use for release with debugging
# set(CMAKE_BUILD_TYPE RelWithDebInfo)

find_package(Threads REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

# Enable std strings in rapidjson
add_definitions(-DRAPIDJSON_HAS_STDSTRING=1)


add_library(interactive   STATIC
        interactive-cpp/source/interactivity.cpp
        interactive-cpp/source/interactivity.h
        )

file(GLOB_RECURSE SRC_DNDPANEL
	"dndpanel/dndpanel/*.cpp"
	"dndpanel/dndpanel/*.h")

source_group("dndpanel" FILES ${SRC_DNDPANEL})

file(GLOB_RECURSE SRC_CHAT
	"dndpanel/Chat/*.cpp"
	"dndpanel/Chat/*.h")

source_group("chat" FILES ${SRC_CHAT})

file(GLOB_RECURSE SRC_CLASSES
	"dndpanel/Classes/*.cpp"
	"dndpanel/Classes/*.h")

source_group("classes" FILES ${SRC_CLASSES})

file(GLOB_RECURSE SRC_PROFESSIONS
	"dndpanel/Professions/*.cpp"
	"dndpanel/Professions/*.h")

source_group("professions" FILES ${SRC_PROFESSIONS})

file(GLOB_RECURSE SRC_QUESTS
	"dndpanel/Quests/*.cpp"
	"dndpanel/Quests/*.h")

source_group("quest" FILES ${SRC_QUESTS})

file(GLOB_RECURSE SRC_USERS
	"dndpanel/Users/*.cpp"
	"dndpanel/Users/*.h")

source_group("users" FILES ${SRC_USERS})

file(GLOB_RECURSE SRC_UTIL
	"dndpanel/Util/*.cpp"
	"dndpanel/Util/*.h")

source_group("util" FILES ${SRC_UTIL})

add_executable(dnd-panel
        ${SRC_DNDPANEL}
		${SRC_CHAT}
		${SRC_CLASSES}
		${SRC_PROFESSIONS}
		${SRC_QUESTS}
		${SRC_USERS}
		${SRC_UTIL}
        )

set(DND_INCLUDES interactive rapidjson/include interactive-cpp/source/)

include_directories(dnd-panel ${DND_INCLUDES})

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb3")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -ggdb3")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ggdb3")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -ggdb3")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -frecord-gcc-switches")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -frecord-gcc-switches")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -frecord-gcc-switches")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -frecord-gcc-switches")

target_link_libraries(dnd-panel interactive ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})