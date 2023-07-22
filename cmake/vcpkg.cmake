# Inspired by: https://blog.kortlepel.com/c++/tutorials/2023/03/16/sdl2-imgui-cmake-vcpkg.html

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/submodules/vcpkg/scripts/buildsystems/vcpkg.cmake")

    if(NOT EXISTS "${CMAKE_TOOLCHAIN_FILE}")
        find_package(Git)
        if(Git_FOUND)
            # shallow-clone vcpkg (we only need the latest revision)
            execute_process(COMMAND "${GIT_EXECUTABLE} submodule update --depth=1 --init --recursive submodules/vcpkg"
                            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                            RESULT_VARIABLE GIT_SUBMODULE_RESULT)
            if(NOT GIT_SUBMODULE_RESULT EQUAL "0")
                message(SEND_ERROR "Checking out vcpkg in source tree failed with ${GIT_SUBMODULE_RESULT}.")
            endif()
        else()
            message(FATAL_ERROR "Could not find git or vcpkg.cmake. Please either, install git and re-run cmake (or run `git submodule update --init --recursive`), or install vcpkg and add `-DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake` to your cmake invocation. Please try again after making those changes.")
        endif()
    endif()
endif()
