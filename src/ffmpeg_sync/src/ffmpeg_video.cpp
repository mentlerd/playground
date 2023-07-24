
#include "ffmpeg_video.hpp"

using namespace ffmpeg;

void Video::open(const char* path) {
    MValue<AVFormatContext> format;

    guard(avformat_open_input(format.cdata(), path, nullptr, nullptr));
    guard(avformat_find_stream_info(format, nullptr));

    MValue<AVCodec> decoderType;
    MValue<AVCodecContext> decoder;

    // TODO: Hm, why does MSVC refuse to implicit cast? This used to work :/
    int stream = av_find_best_stream(format, AVMediaType::AVMEDIA_TYPE_VIDEO, -1, -1, (const AVCodec **) decoderType.cdata(), 0);

    decoder = avcodec_alloc_context3(decoderType);
    decoder->thread_count = 0;

    for (unsigned int idx = 0; idx < format->nb_streams; idx++) {
        if (idx == stream) {
            continue;
        }

        format->streams[idx]->discard = AVDISCARD_ALL;
    }

    guard(avcodec_parameters_to_context(decoder, format->streams[stream]->codecpar));
    guard(avcodec_open2(decoder, decoderType, nullptr));

    {
        this->format = std::move(format);
        this->decoder = std::move(decoder);
        this->stream = stream;
        
        this->packet = av_packet_alloc();
    }
}

FrameResult Video::read(MFrame& into) {
    av_frame_unref(into);

    while (true) {
        int recv = avcodec_receive_frame(decoder, *into);

        if (recv == 0) {
            return FrameResult::OK;
        } else if (recv == AVERROR_EOF) {
            return FrameResult::END;
        } else if (recv == AVERROR(EAGAIN)) {
            while (true) {
                int read = av_read_frame(format, packet);

                if (read == AVERROR_EOF) {
                    return FrameResult::END;
                } else {
                    guard(read);
                }

                if (packet->stream_index != stream) {
                    av_packet_unref(packet);
                    continue;
                } else {
                    guard(avcodec_send_packet(decoder, packet));

                    av_packet_unref(packet);
                    break;
                }
            }
        } else {
            crash(recv);
        }
    }
}

FrameResult Video::readWithNext(MFrame& into) {
    av_frame_unref(into);

    if (buffer) {
        into = std::move(buffer);
    } else {
        read(into);
    }

    auto res = read(buffer);

    if (res == FrameResult::END) {
        return FrameResult::END;
    }

    into.next = buffer->pts;

    return FrameResult::OK;
}

void Video::seek(int64_t pts) {
    avcodec_flush_buffers(decoder);

    guard(avformat_seek_file(format, stream, INT64_MIN, pts, pts, AVSEEK_FLAG_BACKWARD));

    while (true) {
        read(buffer);

        if (buffer->pts >= pts) {
            break;
        }
    }
}

AVRational Video::getTimeBase() const {
    return format->streams[stream]->time_base;
}

const AVRational kAVTimeBaseQ = { 1, AV_TIME_BASE };

int64_t Video::getDuration() const {
    return av_rescale_q(format->duration, kAVTimeBaseQ, getTimeBase());
}

void VideoGraph::open(const Video& video, const std::string& desc) {
    MValue<AVFilterGraph> graph = avfilter_graph_alloc();

    MValue<AVFilterContext> source = avfilter_graph_alloc_filter(graph, avfilter_get_by_name("buffer"), "in");
    {
        MValue<AVBufferSrcParameters> params = av_buffersrc_parameters_alloc();

        params->width  = video.decoder->width;
        params->height = video.decoder->height;
        params->format = video.decoder->pix_fmt;

        params->sample_aspect_ratio = video.decoder->sample_aspect_ratio;
        params->time_base = video.format->streams[video.stream]->time_base;

        guard(av_buffersrc_parameters_set(source, params));
        guard(avfilter_init_str(source, nullptr));
    }

    MValue<AVFilterContext> sink = avfilter_graph_alloc_filter(graph, avfilter_get_by_name("buffersink"), "out");
    {
        guard(avfilter_init_str(source, nullptr));
    }

    MValue<AVFilterInOut> inputs  = avfilter_inout_alloc();
    MValue<AVFilterInOut> outputs = avfilter_inout_alloc();

    inputs->name = av_strdup("out");
    inputs->filter_ctx = sink;
    inputs->pad_idx = 0;
    inputs->next = nullptr;

    outputs->name = av_strdup("in");
    outputs->filter_ctx = source;
    outputs->pad_idx = 0;
    outputs->next = nullptr;

    guard(avfilter_graph_parse_ptr(graph, desc.data(), inputs.cdata(), outputs.cdata(), nullptr));
    guard(avfilter_graph_config(graph, nullptr));

    {
        this->graph = std::move(graph);
        this->source = std::move(source);
        this->sink = std::move(sink);
    }
}
void VideoGraph::close() {
    av_buffersrc_close(source, 0, 0);
}

void VideoGraph::push(MFrame& from) {
    guard(av_buffersrc_add_frame_flags(source, *from, AV_BUFFERSRC_FLAG_KEEP_REF));
}
void VideoGraph::process(MFrame& frame) {
    push(frame);
    pull(frame);
}

FrameResult VideoGraph::pull(MFrame& into) {
    av_frame_unref(into);

    int pull = av_buffersink_get_frame(sink, *into);

    if (pull == 0) {
        return FrameResult::OK;
    } else if (pull == AVERROR_EOF) {
        return FrameResult::END;
    } else if (pull == AVERROR(EAGAIN)) {
        return FrameResult::NEEDS_INPUT;
    } else {
        crash(pull);
    }
}
