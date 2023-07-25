cmake_minimum_required(VERSION 3.26)

# Automatic vcpkg toolchain file injection
include(cmake/vcpkg.cmake)

# This is a toplevel project, we need no languages enabled
project(playground NONE)

if (CMAKE_SYSTEM_NAME STREQUAL "Generic")
	add_subdirectory(src/flipdot_firmware)
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
	add_subdirectory(src/imgui_metal_demo)
else()
	add_subdirectory(src/imgui_demo)
	add_subdirectory(src/ffmpeg_sync)
	add_subdirectory(src/slow_rays)
endif()
