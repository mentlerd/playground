#pragma once

#include "ffmpeg_wrappers.hpp"

#include <string>

namespace ffmpeg {
	
	enum class FrameResult {
		OK, END, NEEDS_INPUT
	};

	class Video;
	class VideoGraph;

	class Video {
		MValue<AVFormatContext> format;
		MValue<AVCodecContext> decoder;

		int stream = -1;

		MValue<AVPacket> packet;
		MFrame buffer;

	public:
		Video() = default;
		
		Video(const char* path) {
			open(path);
		}
		Video(const std::string& path) {
			open(path.data());
		}

		void open(const char* path);
		
		void seek(int64_t pts);

		FrameResult read(MFrame& into);
		FrameResult readWithNext(MFrame& into);

		AVRational getTimeBase() const;
		int64_t getDuration() const;

		friend VideoGraph;
	};

	class VideoGraph {
		MValue<AVFilterGraph> graph;

		MValue<AVFilterContext> source;
		MValue<AVFilterContext> sink;

		MValue<AVFrame> buffer;

	public:
		VideoGraph() = default;
		VideoGraph(const Video& src, const std::string& desc) {
			open(src, desc);
		}

		void open(const Video& src, const std::string& desc);
		void close();

		void push(MFrame& from);
		void process(MFrame& frame);

		FrameResult pull(MFrame& into);
	};

}
