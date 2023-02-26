#ifndef AVEC_BASE_HPP_INCLUDED
#define AVEC_BASE_HPP_INCLUDED
#include <exception>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
}

namespace av {
    // ERROR HANDLING
    // ==============

    class av_category_t final : public std::error_category {
    public:
        inline const char* name() const noexcept override {
            return "av_category";
        }

        inline std::string message(int code) const noexcept override {
            thread_local static char msg_buffer[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(code, msg_buffer, sizeof(msg_buffer));
            return std::string(msg_buffer);
        }
    };

    inline const std::error_category& av_category() {
        static av_category_t result;
        return result;
    }

    // helper function to create a std::system_error.
    inline std::system_error av_error(int code) {
        return std::system_error(code, av_category());
    }

    inline void alloc_video_frame(
        AVFrame* frame, int width, int height, AVPixelFormat pix_fmt
    ) {
        int err;
        if (!av_frame_is_writable(frame) || frame->width != width ||
            frame->height != height || frame->format != pix_fmt) {
            // unref the old data if any
            av_frame_unref(frame);

            // reset parameters
            frame->width  = width;
            frame->height = height;
            frame->format = pix_fmt;

            // reallocate
            if ((err = av_frame_get_buffer(frame, 0)) < 0)
                throw av_error(err);
        }
    }

    inline void alloc_audio_frame(
        AVFrame* frame, int nb_samples, const AVChannelLayout& layout,
        AVSampleFormat sample_fmt
    ) {
        int err = 0;
        if (!av_frame_is_writable(frame) || frame->nb_samples != nb_samples ||
            (err = av_channel_layout_compare(&frame->ch_layout, &layout)) !=
                0) {
            // just in case av_channel_layout_compare throws
            if (err < 0)
                throw av_error(err);

            // unref the old data if any
            av_frame_unref(frame);

            // reset parameters
            frame->nb_samples = nb_samples;
            frame->ch_layout  = layout;
            frame->format     = sample_fmt;

            // reallocate
            if ((err = av_frame_get_buffer(frame, 0)) < 0)
                throw av_error(err);
        }
    }
}  // namespace av

#endif