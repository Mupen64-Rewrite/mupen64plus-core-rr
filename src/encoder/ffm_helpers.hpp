/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus-core - encoder/ffm_helpers.cpp                            *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2012 CasualJames                                        *
 *   Copyright (C) 2009 Richard Goedeken                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
 
#ifndef M64P_ENCODER_FFM_HELPERS_HPP
#define M64P_ENCODER_FFM_HELPERS_HPP
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include "api/m64p_config.h"
#include "api/m64p_types.h"
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
        bool pack = false
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
            if ((err = av_frame_get_buffer(frame, (pack ? 1 : 0))) < 0)
                throw av_error(err);
        }
    }
    
    // For a frame containing video, sets up a pair of arrays
    // that record the same data, vertically flipped.
    inline void setup_vflip_pointers(
        AVFrame* frame, uint8_t* data[AV_NUM_DATA_POINTERS],
        int linesize[AV_NUM_DATA_POINTERS]
    ) {
        for (int i = 0; i < AV_NUM_DATA_POINTERS; i++) {
            data[i] = frame->data[i] + frame->linesize[i] * (frame->height - 1);
            linesize[i] = -frame->linesize[i];
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
        const AVOutputFormat* ofmt, const AVCodec*& vcodec,
        const AVCodec*& acodec
    ) {
        if (strcmp(ofmt->mime_type, "video/x-matroska") == 0) {
            vcodec = avcodec_find_encoder(AV_CODEC_ID_H264);
            acodec = avcodec_find_encoder(AV_CODEC_ID_FLAC);
            return;
        }
        if (strcmp(ofmt->mime_type, "video/webm") == 0) {
            vcodec = avcodec_find_encoder(AV_CODEC_ID_VP9);
            acodec = avcodec_find_encoder(AV_CODEC_ID_OPUS);
            return;
            
        }
        vcodec = (ofmt->video_codec != AV_CODEC_ID_NONE && vcodec)
            ? avcodec_find_encoder(ofmt->video_codec)
            : nullptr;
        acodec = (ofmt->audio_codec != AV_CODEC_ID_NONE && acodec)
            ? avcodec_find_encoder(ofmt->audio_codec)
            : nullptr;
    }
    
    // should be used later for HW accelerators
    inline const AVCodec* find_good_encoder(AVCodecID id) {
        void* it = nullptr;
        const AVCodec* codec, * best_codec = nullptr;
        
        while ((codec = av_codec_iterate(&it))) {
            if (codec->id != id)
                continue;
            
            if (best_codec == nullptr)
                best_codec = codec;
        }
        
        return best_codec;
    }

    inline void sws_setup_frames(
        SwsContext*& sws, AVFrame* src, AVFrame* dst, int flags = SWS_AREA
    ) {
        sws = sws_getCachedContext(
            sws, src->width, src->height,
            static_cast<AVPixelFormat>(src->format), dst->width, dst->height,
            static_cast<AVPixelFormat>(dst->format), flags, nullptr, nullptr,
            nullptr
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
    
    AVDictionary* config_to_dict(m64p_handle handle);
}  // namespace av

#endif