/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus-core - encoder/ffm_encoder.hpp                            *
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
#ifndef M64P_ENCODER_FFM_ENCODER_HPP
#define M64P_ENCODER_FFM_ENCODER_HPP

#include <atomic>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <unordered_map>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include "api/m64p_encoder.h"
#include "api/m64p_types.h"
}

namespace m64p {
    using config_dict = std::unordered_map<std::string, std::string>;

    class unsupported_error : std::exception {
    public:
        using std::exception::exception;
    };

    class ffm_encoder {
    public:
        struct config {
            std::unordered_map<std::string, std::string> format_opts;
            std::unordered_map<std::string, std::string> video_opts;
            std::unordered_map<std::string, std::string> audio_opts;
        };

        ffm_encoder(const char* path, m64p_encoder_format fmt);

        ~ffm_encoder();

        void read_screen();
        void push_video();

        void set_sample_rate(unsigned int rate);

        void push_audio(const void* samples, size_t len);

        void finish(bool discard);

    private:
        ffm_encoder(
            const char* path, m64p_encoder_format fmt, const config* cfg
        );

        AVFormatContext* m_fmt_ctx;

        AVStream* m_vstream;
        std::atomic_int64_t m_vpts;
        AVCodecContext* m_vcodec_ctx;
        const AVCodec* m_vcodec;
        AVPacket* m_vpacket;
        AVFrame* m_vframe1;
        AVFrame* m_vframe2;
        SwsContext* m_sws;
        std::mutex m_vmutex;
        std::condition_variable m_vcond;
        std::atomic_bool m_vflag;

        AVStream* m_astream;
        std::atomic_int64_t m_apts;
        AVCodecContext* m_acodec_ctx;
        const AVCodec* m_acodec;
        AVPacket* m_apacket;
        AVFrame* m_aframe1;
        AVFrame* m_aframe2;
        SwrContext* m_swr;
        int m_aframe_size;
        std::mutex m_amutex;

        void video_init();
        void audio_init();
    };

}  // namespace m64p

extern "C" struct encoder_backend_interface g_iffmpeg_encoder_backend;

#endif