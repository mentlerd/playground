cmake_minimum_required(VERSION 3.26)

find_program(MAKEBIN makebin REQUIRED)

project(flipdot_firmware C)

add_executable(firmware
	src/main.c
)

target_compile_options(firmware PRIVATE
	# Treat all warnings as errors
	"--Werror"

	# Generate code for the Intel MCS51 family of processors
	"-mmcs51"

	# All functions in the source file will be compiled as reentrant (until code size/speed is an issue)
	"--stack-auto"
)

set_target_properties(firmware PROPERTIES
	C_STANDARD 11
	C_STANDARD_REQUIRED YES
	C_EXTENSIONS YES
)

add_custom_command(
	OUTPUT ${PROJECT_BINARY_DIR}/firmware.bin
	COMMAND ${MAKEBIN}
	ARGS -s 0x8000 $<TARGET_FILE:firmware> ${PROJECT_BINARY_DIR}/firmware.bin
)

add_custom_target(firmware_bin ALL DEPENDS ${PROJECT_BINARY_DIR}/firmware.bin)
