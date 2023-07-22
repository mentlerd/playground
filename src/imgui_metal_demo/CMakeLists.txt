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
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER  "com.example.someapp"
    XCODE_ATTRIBUTE_PRODUCT_NAME               "SomeApp"
    XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION    "1.0"
    XCODE_ATTRIBUTE_MARKETING_VERSION          "1.0"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY     "1,2"

    XCODE_ATTRIBUTE_VALIDATE_PRODUCT[variant=Release]       YES
    XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME      AppIcon
    XCODE_ATTRIBUTE_GENERATE_INFOPLIST_FILE                 YES
    XCODE_ATTRIBUTE_INFOPLIST_FILE                          ""
    XCODE_ATTRIBUTE_INFOPLIST_KEY_CFBundleDisplayName       "Some name to show the user"
    XCODE_ATTRIBUTE_INFOPLIST_KEY_NSHumanReadableCopyright  "(c)2023 Example Co."

    XCODE_ATTRIBUTE_INFOPLIST_KEY_NSHighResolutionCapable YES
    XCODE_ATTRIBUTE_INFOPLIST_KEY_UIApplicationSupportsIndirectInputEvents YES
    XCODE_ATTRIBUTE_INFOPLIST_KEY_UISupportedInterfaceOrientations_iPad   "UIInterfaceOrientationPortrait"
    XCODE_ATTRIBUTE_INFOPLIST_KEY_UISupportedInterfaceOrientations_iPhone "UIInterfaceOrientationPortrait"
)
