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

add_executable(dnd-panel
        dndpanel/common.h
        dndpanel/main.cpp
        dndpanel/main.h
        dndpanel/logger.cpp
        dndpanel/logger.h
        dndpanel/config.cpp
        dndpanel/config.h
        dndpanel/panellogic.cpp
        dndpanel/panellogic.h
		dndpanel/chatutil.cpp
		dndpanel/chatutil.h
		dndpanel/chatbot.cpp
		dndpanel/chatbot.h
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