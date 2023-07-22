cmake_minimum_required(VERSION 3.26)

project(imgui_demo CXX)

find_package(SDL2 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(imgui_demo WIN32 MACOSX_BUNDLE
	src/main.cpp
)

target_link_libraries(imgui_demo 
PRIVATE 
	$<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
	$<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>

	OpenGL::GL
	imgui::imgui
)
