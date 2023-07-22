cmake_minimum_required(VERSION 3.26)

project(imgui_metal_demo CXX OBJCXX)

find_package(SDL2 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

add_executable(imgui_metal_demo WIN32 MACOSX_BUNDLE
	src/main.mm
)

target_link_libraries(imgui_metal_demo 
PRIVATE 
	$<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
	$<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>

	imgui::imgui
)

set_target_properties(imgui_metal_demo PROPERTIES
	MACOSX_BUNDLE_BUNDLE_NAME foo
	MACOSX_BUNDLE_BUNDLE_VERSION 0.1
	MACOSX_BUNDLE_SHORT_VERSION_STRING 0.1
	MACOSX_BUNDLE_GUI_IDENTIFIER foo.bar
)
