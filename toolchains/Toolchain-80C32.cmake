# See: 
#  https://cmake.org/cmake/help/book/mastering-cmake/chapter/Cross%20Compiling%20With%20CMake.html
#  https://cmake.org/cmake/help/book/mastering-cmake/chapter/Cross%20Compiling%20With%20CMake.html#cross-compiling-for-a-microcontroller
#  https://github.com/Kitware/CMake/blob/master/Modules/Platform/Generic-SDCC-C.cmake

get_filename_component(_ownDir "${CMAKE_CURRENT_LIST_FILE}" PATH)

set(CMAKE_MODULE_PATH "${_ownDir}/Modules" ${CMAKE_MODULE_PATH})

set(CMAKE_SYSTEM_NAME      "Generic")
set(CMAKE_SYSTEM_PROCESSOR "80C32")

set(CMAKE_FIND_ROOT_PATH "c:/Program Files/SDCC")
set(CMAKE_C_COMPILER     "c:/Program Files/SDCC/bin/sdcc.exe")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
