#pragma once

#include "ffmpeg.hpp"

#include <utility>

namespace ffmpeg {

	namespace detail {
		inline void free(AVFrame** ptr) { av_frame_free(ptr); }
		inline void free(AVPacket** ptr) { av_packet_free(ptr); };

		inline void free(AVFormatContext** ptr) { avformat_free_context(*ptr); }
		inline void free(AVCodecContext** ptr) { avcodec_free_context(ptr); };

		inline void free(AVFilterGraph** ptr) { avfilter_graph_free(ptr); };
		inline void free(AVFilterInOut** ptr) { avfilter_inout_free(ptr); };

		inline void free(AVBufferSrcParameters** ptr) { av_free(*ptr); }

		// In .data section, or freed by owner
		inline void free(AVCodec** ptr) {};
		inline void free(AVFilterContext** ptr) {};
	}

	void crash [[noreturn]] (int errcode);
	void guard(int errcode);

	template<typename T>
	T* guard(T* value) {
		if (value == nullptr) {
			crash(0);
		}

		return value;
	}


	template<typename T>
	struct MValue {
		
		MValue() {
		}
		MValue(T* value) {
			ptr = guard(value);
		}

		void operator=(MValue&& other) {
			if (this != &other) {
				std::swap(ptr, other.ptr);
			}
		}

		T* operator*() {
			return ptr;
		}
		const T* operator*() const {
			return ptr;
		}

		T* operator->() { 
			return ptr; 
		}
		const T* operator->() const {
			return ptr; 
		}

		operator T* () {
			return ptr;
		}
		operator const T* () const {
			return ptr;
		}

		T** cdata() {
			return &ptr;
		}

		~MValue() {
			detail::free(&ptr);
		}

	protected:
		T* ptr = nullptr;
	};

	struct MFrame : public MValue<AVFrame> {

		MFrame() : MValue() {
		}
		MFrame(const MFrame& other) {
			*this = other;
		}
		MFrame(MFrame&& victim) noexcept {
			*this = std::move(victim);
		}

		void operator=(const MFrame& other) {
			if (this != &other) {
				av_frame_unref(ptr);

				if (other.ptr) {
					av_frame_ref(alloc(), other.ptr);
				}

				next = other.next;
			}
		}
		void operator=(MFrame&& other) noexcept {
			if (this != &other) {
				std::swap(ptr, other.ptr);
				std::swap(next, other.next);
			}
		}

		void free() {
			detail::free(&ptr);
		}

		operator bool() const {
			return ptr != nullptr;
		}

		AVFrame* operator*() { 
			return alloc(); 
		}
		AVFrame* operator->() { 
			return alloc(); 
		}

		const AVFrame* operator->() const {
			return ptr;
		}

		int64_t next = 0;

		int64_t start() const {
			return ptr->pts;
		}
		int64_t end() const {
			return next;
		}

	private:
		AVFrame* alloc() {
			if (ptr == nullptr) {
				ptr = av_frame_alloc();
			}

			return ptr;
		}
	};

}
