#ifndef FFM_HELPERS_HPP
#define FFM_HELPERS_HPP
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
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

    // Allocate or reallocate buffers in a video frame.
    inline void alloc_video_frame(
        AVFrame* frame, int width, int height, AVPixelFormat pix_fmt,
        bool align = false
    ) {
        int err;
        if (!av_frame_is_writable(frame) || frame->width != width ||
            frame->height != height || frame->format != pix_fmt) {
            // unref the old data if any
            if (frame->buf[0] != nullptr) {
                av_frame_unref(frame);
            }

            // reset parameters
            frame->width  = width;
            frame->height = height;
            frame->format = pix_fmt;

            // reallocate
            if ((err = av_frame_get_buffer(frame, 0)) < 0)
                throw av_error(err);
        }
    }

    // Allocate or reallocate buffers in an audio frame.
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
            if (frame->buf[0]) {
                av_frame_unref(frame);
            }

            // reset parameters
            frame->nb_samples = nb_samples;
            frame->ch_layout  = layout;
            frame->format     = sample_fmt;

            // reallocate
            if ((err = av_frame_get_buffer(frame, 0)) < 0)
                throw av_error(err);
        }
    }

    // Encode the frame f using codec context c, by way of packet p, and stream
    // s.
    inline void encode_and_write(
        AVFrame* f, AVPacket* p, AVCodecContext* c, AVStream* s,
        AVFormatContext* d
    ) {
        int err;
        // send the frame for encoding
        if ((err = avcodec_send_frame(c, f)) < 0)
            throw av::av_error(err);

        // loop until we can't get any more packets
        while ((err = avcodec_receive_packet(c, p)) >= 0) {
            // set stream_index and PTS
            av_packet_rescale_ts(p, c->time_base, s->time_base);
            p->stream_index = s->index;
            // write the frame
            if ((err = av_interleaved_write_frame(d, p)) < 0)
                throw av::av_error(err);
        }
        // AVERROR(EAGAIN): you need to send another frame
        // AVERROR_EOF: encoder is done
        // anything else: invalid reason, throw now
        if (err != AVERROR(EAGAIN) && err != AVERROR_EOF)
            throw av::av_error(err);
    }

    inline void select_codecs(
        const AVOutputFormat* ofmt, const AVCodec*& vcodec, const AVCodec*& acodec
    ) {
        if (strcmp(ofmt->mime_type, "video/x-matroska") == 0) {
            vcodec = avcodec_find_encoder(AV_CODEC_ID_VP8);
            acodec = avcodec_find_encoder(AV_CODEC_ID_VORBIS);
            return;
        }
        vcodec = (ofmt->video_codec != AV_CODEC_ID_NONE && vcodec)
            ? avcodec_find_encoder(ofmt->video_codec)
            : nullptr;
        acodec = (ofmt->audio_codec != AV_CODEC_ID_NONE && acodec)
            ? avcodec_find_encoder(ofmt->audio_codec)
            : nullptr;
    }
    
    inline void sws_setup_frames(SwsContext*& sws, AVFrame* src, AVFrame* dst, int flags = SWS_AREA) {
        sws = sws_getCachedContext(sws,
            src->width, src->height, static_cast<AVPixelFormat>(src->format),
            dst->width, dst->height, static_cast<AVPixelFormat>(dst->format),
            flags, nullptr, nullptr, nullptr
        );
    }
    
    inline void save_frame(const AVFrame* frame, const char* path) {
        if (!frame || frame->format != AV_PIX_FMT_RGB24)
            throw std::invalid_argument("Invalid or null frame");
        FILE* f = fopen(path, "wb");
        fprintf(f, "P6 %d %d 255\n", frame->width, frame->height);
        fwrite(frame->data[0], frame->linesize[0] * frame->height, 1, f);
        fflush(f);
        fclose(f);
    }

    template <auto P>
    struct fn_delete {
        void operator()(void* p) { P(p); }
    };
}  // namespace av

#endif