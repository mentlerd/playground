cmake_minimum_required(VERSION 3.26)

project(flipdot_firmware C)

add_executable(firmware
	src/main.c
)

set_target_properties(firmware PROPERTIES
	C_STANDARD 11
	C_STANDARD_REQUIRED YES
	C_EXTENSIONS NO
)
