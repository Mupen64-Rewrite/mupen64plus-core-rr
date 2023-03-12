/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus-core - encoder/ffm_encoder.cpp                            *
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

/*
Shoutouts to https://github.com/leandromoreira/ffmpeg-libav-tutorial for
helping me through this pain of a library. Much of the init code is from there.

Just so I don't get sued:
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#define M64P_CORE_PROTOTYPES 1
#include <cstdio>
#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <system_error>
#include "ffm_encoder.hpp"
#include "ffm_helpers.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include "api/callbacks.h"
#include "api/m64p_encoder.h"
#include "api/m64p_types.h"
#include "api/m64p_config.h"
#include "plugin/plugin.h"
}

using namespace std::literals;

static const char* fmt_mime_types[] = {"video/mp4", "video/webm"};
static AVChannelLayout chl_stereo AV_CHANNEL_LAYOUT_STEREO;

namespace {
    // Automatically runs a function on scope exit,
    // much like a try/finally in Java/C#.
    template <class F>
    struct finally {
        finally(F&& f) : f(f) { static_assert(noexcept(f())); }
        ~finally() { f(); }
        F f;
    };

}  // namespace

namespace m64p {

    ffm_encoder::ffm_encoder(const char* path, m64p_encoder_format fmt) :
        m_vflag(false) {
        int err;
        const AVOutputFormat* ofmt;

        // guess output format
        if (fmt == M64FMT_INFER)
            ofmt = av_guess_format(NULL, path, NULL);
        else
            ofmt = av_guess_format(NULL, NULL, fmt_mime_types[fmt]);

        if (ofmt == nullptr)
            throw std::runtime_error("No available output formats");

        // alloc output context
        if ((err = avformat_alloc_output_context2(&m_fmt_ctx, ofmt, NULL, path)
            ) < 0)
            throw av::av_error(err);

        // find codecs
        
        av::select_codecs(ofmt, m_vcodec, m_acodec);

        // alloc and init video stream
        if (m_vcodec) {
            m_vstream    = avformat_new_stream(m_fmt_ctx, m_vcodec);
            m_vcodec_ctx = avcodec_alloc_context3(m_vcodec);
            m_vpacket    = av_packet_alloc();
            m_vframe1    = av_frame_alloc();
            m_vframe2    = av_frame_alloc();

            if (!m_vstream || !m_vcodec_ctx || !m_vpacket || !m_vframe1 ||
                !m_vframe2) {
                throw std::bad_alloc();
            }
            video_init();
        }
        else {
            m_vstream    = nullptr;
            m_vcodec_ctx = nullptr;
            m_vpacket    = nullptr;
            m_vframe1    = nullptr;
        }
        m_sws  = nullptr;
        m_vpts = 0;

        // alloc and init audio stream
        if (m_acodec) {
            m_astream    = avformat_new_stream(m_fmt_ctx, m_acodec);
            m_acodec_ctx = avcodec_alloc_context3(m_acodec);
            m_apacket    = av_packet_alloc();
            m_aframe1    = av_frame_alloc();
            m_aframe2    = av_frame_alloc();

            if (!m_astream || !m_acodec_ctx || !m_apacket || !m_aframe1 ||
                !m_aframe2) {
                throw std::bad_alloc();
            }
            audio_init();
        }
        else {
            m_astream    = nullptr;
            m_acodec_ctx = nullptr;
            m_apacket    = nullptr;
            m_aframe1    = nullptr;
        }
        m_apts = 0;

        // set a global header if the format demands one
        if (m_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            m_fmt_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        // open a file
        if (!(m_fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
            if ((err = avio_open(&m_fmt_ctx->pb, path, AVIO_FLAG_WRITE)) < 0) {
                throw av::av_error(err);
            }
        }

        {
            m64p_handle cfg_sect;
            AVDictionary* dict = nullptr;
            finally _guard([&]() noexcept {
                av_dict_free(&dict);
            });
            // I know people avoid goto like the plague, but it's the best
            // option here
            if ((err = ConfigOpenSection("EncFFmpeg-Format", &cfg_sect)) !=
                M64ERR_SUCCESS) {
                DebugMessage(
                    M64MSG_WARNING,
                    "Failed to open format config, using defaults"
                );
                goto ffm_encoder_0_ffm_encoder_0_write_header;
            }
            dict = av::config_to_dict(cfg_sect);
            if (dict == nullptr) {
                DebugMessage(
                    M64MSG_WARNING,
                    "No format config options found, using defaults"
                );
            }
            
            ffm_encoder_0_ffm_encoder_0_write_header:
            // write the header
            if ((err = avformat_write_header(m_fmt_ctx, &dict)) < 0)
                throw av::av_error(err);
        }
    }

    ffm_encoder::~ffm_encoder() {
        if (m_vcodec != nullptr) {
            avcodec_free_context(&m_vcodec_ctx);
            av_packet_free(&m_vpacket);
            av_frame_free(&m_vframe1);
            av_frame_free(&m_vframe2);
            sws_freeContext(m_sws);
            m_sws = nullptr;
        }
        if (m_acodec != nullptr) {
            avcodec_free_context(&m_acodec_ctx);
            av_packet_free(&m_apacket);
            av_frame_free(&m_aframe1);
            av_frame_free(&m_aframe2);
            swr_free(&m_swr);
        }

        avformat_free_context(m_fmt_ctx);
        m_fmt_ctx = nullptr;
    }

    void ffm_encoder::video_init() {
        int err;

        // Setup private data
        av_opt_set(m_vcodec_ctx->priv_data, "preset", "fast", 0);
        // width/height (this will come from config later)
        m_vcodec_ctx->width        = 640;
        m_vcodec_ctx->coded_width  = 640;
        m_vcodec_ctx->height       = 480;
        m_vcodec_ctx->coded_height = 480;
        // pixel aspect ratio (they're square)
        m_vcodec_ctx->sample_aspect_ratio = {1, 1};
        // pixel format (use YCbCr 4:2:0 if not known)
        if (m_vcodec->pix_fmts)
            m_vcodec_ctx->pix_fmt = m_vcodec->pix_fmts[0];
        else
            m_vcodec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
        // bitrate settings (reasonable defaults, I guess)
        m_vcodec_ctx->bit_rate       = 2e6;
        m_vcodec_ctx->rc_buffer_size = 4e6;
        m_vcodec_ctx->rc_max_rate    = 2e6;
        m_vcodec_ctx->rc_min_rate    = 2.5e6;
        // time_base = 1/frame_rate
        m_vcodec_ctx->time_base = {1, 60};
        m_vstream->time_base    = {1, 60};
        {
            m64p_handle cfg_sect;
            AVDictionary* dict = nullptr;
            finally _guard([&]() noexcept {
                av_dict_free(&dict);
            });
            // I know people avoid goto like the plague, but it's the best
            // option here
            if ((err = ConfigOpenSection("EncFFmpeg-Video", &cfg_sect)) !=
                M64ERR_SUCCESS) {
                DebugMessage(
                    M64MSG_WARNING,
                    "Failed to open video config, using defaults"
                );
                goto ffm_encoder_0_video_init_0_open_codec;
            }
            dict = av::config_to_dict(cfg_sect);
            if (dict == nullptr) {
                DebugMessage(
                    M64MSG_WARNING,
                    "No video config options found, using defaults"
                );
            }

        ffm_encoder_0_video_init_0_open_codec:
            // open the codec
            if ((err = avcodec_open2(m_vcodec_ctx, m_vcodec, &dict)) < 0)
                throw av::av_error(err);
        }
        // set codec parameters on stream
        if ((err = avcodec_parameters_from_context(
                 m_vstream->codecpar, m_vcodec_ctx
             )) < 0)
            throw av::av_error(err);

        // allocate and fill vframe2 with black
        // this is for the synchronizer
        av::alloc_video_frame(
            m_vframe2, m_vcodec_ctx->width, m_vcodec_ctx->height,
            m_vcodec_ctx->pix_fmt
        );
        {
            ptrdiff_t linesizes[AV_NUM_DATA_POINTERS];
            std::copy_n(m_vframe2->linesize, AV_NUM_DATA_POINTERS, linesizes);
            err = av_image_fill_black(
                m_vframe2->data, linesizes, m_vcodec_ctx->pix_fmt,
                m_vcodec_ctx->color_range, m_vcodec_ctx->width,
                m_vcodec_ctx->height
            );
            if (err < 0)
                throw av::av_error(err);
        }
    }

    void ffm_encoder::audio_init() {
        int err;

        // sample rate and bit rate are very sensible defaults
        m_acodec_ctx->ch_layout   = AV_CHANNEL_LAYOUT_STEREO;
        m_acodec_ctx->sample_rate = 48000;
        if (m_acodec->sample_fmts) {
            AVSampleFormat fmt = m_acodec->sample_fmts[0];
            for (const auto* i = m_acodec->sample_fmts; *i != -1; i++) {
                if (*i == AV_SAMPLE_FMT_S16) {
                    fmt = *i;
                    break;
                }
                if (*i == AV_SAMPLE_FMT_S16P)
                    fmt = *i;
            }
            DebugMessage(
                M64MSG_INFO, "Found sample format: %s",
                av_get_sample_fmt_name(fmt)
            );
            m_acodec_ctx->sample_fmt = fmt;
        }
        else
            m_acodec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
        m_acodec_ctx->bit_rate = 196000;
        // time_base = 1/sample_rate
        m_acodec_ctx->time_base = {1, m_acodec_ctx->sample_rate};
        m_astream->time_base    = m_acodec_ctx->time_base;

        // no idea why this is here, but I'm all for it
        m_acodec_ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

        // open the codec
        {
            m64p_handle cfg_sect;
            AVDictionary* dict = nullptr;
            finally _guard([&]() noexcept {
                av_dict_free(&dict);
            });
            // I know people avoid goto like the plague, but it's the best
            // option here
            if ((err = ConfigOpenSection("EncFFmpeg-Audio", &cfg_sect)) !=
                M64ERR_SUCCESS) {
                DebugMessage(
                    M64MSG_WARNING,
                    "Failed to open audio config, using defaults"
                );
                goto ffm_encoder_0_audio_init_0_open_codec;
            }
            dict = av::config_to_dict(cfg_sect);
            if (dict == nullptr) {
                DebugMessage(
                    M64MSG_WARNING,
                    "No audio config options found, using defaults"
                );
            }
            
            ffm_encoder_0_audio_init_0_open_codec:
            if ((err = avcodec_open2(m_acodec_ctx, m_acodec, NULL)) < 0)
                throw av::av_error(err);
        }
        // set codec parameters on stream
        if ((err = avcodec_parameters_from_context(
                 m_astream->codecpar, m_acodec_ctx
             )) < 0)
            throw av::av_error(err);

        // if the frame size is 0, change it to something reasonable
        if (m_acodec_ctx->frame_size == 0)
            m_aframe_size = 4096;
        else
            m_aframe_size = m_acodec_ctx->frame_size;

        // Setup the swresample context with a default sample rate
        m_swr = nullptr;
        err   = swr_alloc_set_opts2(
            &m_swr,
            // dst settings
            &m_acodec_ctx->ch_layout, m_acodec_ctx->sample_fmt,
            m_acodec_ctx->sample_rate,
            // src settings
            &chl_stereo, AV_SAMPLE_FMT_S16, 44100, 0, nullptr
        );
        if (err < 0)
            throw av::av_error(err);

        err = av_opt_set_int(m_swr, "dither_method", SWR_DITHER_TRIANGULAR, 0);
        if (err < 0)
            throw av::av_error(err);

        if ((err = swr_init(m_swr)) < 0)
            throw av::av_error(err);
    }

    void ffm_encoder::read_screen() {
        int fw = 0, fh = 0;
        {
            std::unique_lock _lock(m_vmutex);
            while (m_vflag)
                m_vcond.wait(_lock);
            m_vflag = true;
        }

        // check what the frame size is
        gfx.readScreen(nullptr, &fw, &fh, false);
        if (fw == 0 || fh == 0)
            throw std::logic_error("invalid frame size!");
        av::alloc_video_frame(m_vframe1, fw, fh, AV_PIX_FMT_RGB24, true);
        // read the screen data (fw/fh aren't useful anymore)
        gfx.readScreen(m_vframe1->data[0], &fw, &fh, false);
    }

    void ffm_encoder::push_video() {
        if (!m_vcodec)
            return;

        finally _scope([&]() noexcept {
            std::unique_lock _lock(m_vmutex);
            m_vflag = false;
            m_vcond.notify_one();
        });

        int err = 0;

        while (av_compare_ts(
                   m_vpts, m_vcodec_ctx->time_base, m_apts,
                   m_acodec_ctx->time_base
               ) < 0) {
            m_vframe2->pts = m_vpts++;
            av::encode_and_write(
                m_vframe2, m_vpacket, m_vcodec_ctx, m_vstream, m_fmt_ctx
            );
        }

        // (re)allocate FFmpeg structs
        av::alloc_video_frame(
            m_vframe2, m_vcodec_ctx->width, m_vcodec_ctx->height,
            m_vcodec_ctx->pix_fmt
        );
        av::sws_setup_frames(m_sws, m_vframe1, m_vframe2);
        {
            // setup image unflipping
            uint8_t* flip_data[AV_NUM_DATA_POINTERS];
            int flip_linesize[AV_NUM_DATA_POINTERS];
            av::setup_vflip_pointers(m_vframe1, flip_data, flip_linesize);
            err = sws_scale(
                m_sws, flip_data, flip_linesize, 0, m_vframe1->height,
                m_vframe2->data, m_vframe2->linesize
            );
        }
        if (err < 0)
            throw av::av_error(err);

        // set the timestamp for this frame
        m_vframe2->time_base = m_vcodec_ctx->time_base;
        m_vframe2->pts       = m_vpts++;

        av::encode_and_write(
            m_vframe2, m_vpacket, m_vcodec_ctx, m_vstream, m_fmt_ctx
        );
    }

    void ffm_encoder::set_sample_rate(unsigned int rate) {
        std::lock_guard _lock(m_amutex);
        int err;
        // change settings and reinitialize
        if (m_swr && swr_is_initialized(m_swr))
            swr_close(m_swr);
        err = swr_alloc_set_opts2(
            &m_swr,
            // dst settings
            &m_acodec_ctx->ch_layout, m_acodec_ctx->sample_fmt,
            m_acodec_ctx->sample_rate,
            // src settings
            &chl_stereo, AV_SAMPLE_FMT_S16, rate, 0, nullptr
        );
        if (err < 0)
            throw av::av_error(err);
        if ((err = swr_init(m_swr)) < 0)
            throw av::av_error(err);
    }

    void ffm_encoder::push_audio(const void* samples, size_t len) {
        std::lock_guard _lock(m_amutex);
        if (!m_acodec)
            return;
        int err;
        size_t ilen;
        // Initial allocations
        ilen = len / 4;
        av::alloc_audio_frame(m_aframe1, ilen, chl_stereo, AV_SAMPLE_FMT_S16);
        int16_t *p1 = (int16_t*) samples, *p2 = (int16_t*) m_aframe1->data[0];

        for (size_t i = 0; i < ilen * 2; i += 2) {
            int16_t s1 = p1[i], s2 = p1[i + 1];
#ifndef M64P_BIG_ENDIAN
            std::swap(s1, s2);
#endif
#if 0
            // Hard saturation to prevent any AAC issues
            p2[i] = std::clamp(
                s1, static_cast<int16_t>(-16384), static_cast<int16_t>(16384)
            );
            p2[i + 1] = std::clamp(
                s2, static_cast<int16_t>(-16384), static_cast<int16_t>(16384)
            );
#else
            p2[i] = s1, p2[i + 1] = s2;
#endif
        }
        // push samples into resampler
        err = swr_convert(
            m_swr, nullptr, 0, const_cast<const uint8_t**>(m_aframe1->data),
            ilen
        );
        if (err < 0)
            throw av::av_error(err);
        // extract complete frames from resampler
        while (swr_get_out_samples(m_swr, 0) >= m_aframe_size) {
            av::alloc_audio_frame(
                m_aframe2, m_aframe_size, m_acodec_ctx->ch_layout,
                m_acodec_ctx->sample_fmt
            );
            err = swr_convert(
                m_swr, m_aframe2->data, m_aframe2->nb_samples, nullptr, 0
            );
            if (err < 0)
                throw av::av_error(err);

            m_aframe2->pts = m_apts;
            m_apts += m_aframe2->nb_samples;

            av::encode_and_write(
                m_aframe2, m_apacket, m_acodec_ctx, m_astream, m_fmt_ctx
            );
        }
    }

    void ffm_encoder::finish(bool discard) {
        int err;
        int nb_samples;
        if (m_vcodec) {
            // flush packets
            av::encode_and_write(
                nullptr, m_vpacket, m_vcodec_ctx, m_vstream, m_fmt_ctx
            );
        }

        if (m_acodec) {
            // drain last bytes from resampler
            nb_samples = swr_get_out_samples(m_swr, 0);
            av::alloc_audio_frame(
                m_aframe2, nb_samples, m_acodec_ctx->ch_layout,
                m_acodec_ctx->sample_fmt
            );
            {
                std::lock_guard _lock(m_amutex);
                err =
                    swr_convert(m_swr, m_aframe2->data, nb_samples, nullptr, 0);
                if (err < 0)
                    throw av::av_error(err);
            }

            m_aframe2->pts = m_apts;
            m_apts += m_aframe2->nb_samples;

            av::encode_and_write(
                m_aframe2, m_apacket, m_acodec_ctx, m_astream, m_fmt_ctx
            );
            // flush packets.
            // AAC is not supported here due to issues with the encoder.
            if (m_acodec->id != AV_CODEC_ID_AAC) {
                av::encode_and_write(
                    nullptr, m_apacket, m_acodec_ctx, m_astream, m_fmt_ctx
                );
            }
        }

        // write the file trailer
        if ((err = av_write_trailer(m_fmt_ctx)) < 0)
            throw av::av_error(err);
    }

}  // namespace m64p
