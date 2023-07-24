
#include "ffmpeg_wrappers.hpp"

#include <exception>
#include <iostream>

void ffmpeg::crash(int errcode) {
	char buffer[AV_ERROR_MAX_STRING_SIZE];
	av_make_error_string(buffer, sizeof(buffer), errcode);

	std::cerr << "Fatal error: " << buffer << std::endl;
	std::terminate();
}

void ffmpeg::guard(int errcode) {
	if (errcode >= 0) {
		return;
	}
	
	crash(errcode);
}
