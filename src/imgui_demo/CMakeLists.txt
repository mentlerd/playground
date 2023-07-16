cmake_minimum_required(VERSION 3.26)

project(imgui_demo CXX)

find_package(imgui REQUIRED)
find_package(OpenGL REQUIRED COMPONENTS OpenGL)
find_package(SDL2 REQUIRED COMPONENTS SDL2 SDL2main)

add_executable(imgui_demo WIN32
	src/main.cpp
)

target_link_libraries(imgui_demo 
PRIVATE 
	SDL2::SDL2main
	SDL2::SDL2
	OpenGL::GL
	imgui::imgui
)
