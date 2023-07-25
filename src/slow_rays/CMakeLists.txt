cmake_minimum_required(VERSION 3.26)

project(slow_rays CXX)

find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)

add_executable(slow_rays
	src/main.cpp
)

target_link_libraries(slow_rays 
PRIVATE 
	OpenGL::GL
	GLUT::GLUT
)
