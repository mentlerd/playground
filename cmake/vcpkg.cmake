if (CMAKE_TOOLCHAIN_FILE)
    set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_TOOLCHAIN_FILE}")
endif()

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/submodules/vcpkg/scripts/buildsystems/vcpkg.cmake")

# Prevent infinite recursion in case the toolchain file is explicitly specified
if (VCPKG_CHAINLOAD_TOOLCHAIN_FILE STREQUAL CMAKE_TOOLCHAIN_FILE)
    unset(VCPKG_CHAINLOAD_TOOLCHAIN_FILE)
endif()
