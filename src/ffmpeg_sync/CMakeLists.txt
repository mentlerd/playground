cmake_minimum_required(VERSION 3.26)

if (NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
	return()
endif()

project(ffmpeg_sync CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(FFMPEG REQUIRED)

add_executable(ffmpeg_sync
	src/ffmpeg.hpp
	src/ffmpeg_video.hpp
	src/ffmpeg_video.cpp
	src/ffmpeg_wrappers.hpp
	src/ffmpeg_wrappers.cpp

	src/main.cpp
)

# TODO: vcpkg recommends this, not quite sure it is the right way tho
target_include_directories(ffmpeg_sync PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_directories(ffmpeg_sync PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries(ffmpeg_sync PRIVATE ${FFMPEG_LIBRARIES})
