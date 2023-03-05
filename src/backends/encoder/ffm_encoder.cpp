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

#include "ffm_encoder.hpp"
#include <stdio.h>
#include <algorithm>
#include <array>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <system_error>
#include "backends/api/encoder.h"
#include "ffm_helpers.hpp"
#include "plugin/plugin.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include "api/callbacks.h"
#include "api/m64p_types.h"
}

using namespace std::literals;

static const char* fmt_mime_types[] = {"video/mp4", "video/webm"};
static AVChannelLayout chl_stereo = (AVChannelLayout) AV_CHANNEL_LAYOUT_STEREO;

namespace {}

namespace m64p {

    ffm_encoder::ffm_encoder(const char* path, m64p_encoder_format fmt) {
        int err;
        const AVOutputFormat* ofmt;

        // guess output format
        if (fmt == M64FMT_NULL)
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
        m_swr  = nullptr;
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

        // write the header (formatting options go here)
        if ((err = avformat_write_header(m_fmt_ctx, NULL)) < 0)
            throw av::av_error(err);
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

        // open the codec
        if ((err = avcodec_open2(m_vcodec_ctx, m_vcodec, NULL)) < 0)
            throw av::av_error(err);
        // set codec parameters on stream
        if ((err = avcodec_parameters_from_context(
                 m_vstream->codecpar, m_vcodec_ctx
             )) < 0)
            throw av::av_error(err);
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
        if ((err = avcodec_open2(m_acodec_ctx, m_acodec, NULL)) < 0)
            throw av::av_error(err);
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
        err = swr_alloc_set_opts2(
            &m_swr,
            // dst settings
            &m_acodec_ctx->ch_layout, m_acodec_ctx->sample_fmt,
            m_acodec_ctx->sample_rate,
            // src settings
            &chl_stereo, AV_SAMPLE_FMT_S16, 44100, 0, nullptr
        );
        if (err < 0)
            throw av::av_error(err);
        if ((err = swr_init(m_swr)) < 0)
            throw av::av_error(err);
    }

    void ffm_encoder::push_video() {
        if (!m_vcodec)
            return;

        int err = 0;
        int fw = 0, fh = 0;
        // check what the frame size is
        gfx.readScreen(nullptr, &fw, &fh, false);
        if (fw == 0 || fh == 0)
            throw std::logic_error("invalid frame size!");

        // (re)allocate FFmpeg structs
        av::alloc_video_frame(m_vframe1, fw, fh, AV_PIX_FMT_RGB24);
        av::alloc_video_frame(
            m_vframe2, m_vcodec_ctx->width, m_vcodec_ctx->height,
            m_vcodec_ctx->pix_fmt
        );
        av::sws_setup_frames(m_sws, m_vframe1, m_vframe2);

        // read the screen data (fw/fh aren't useful anymore)
        gfx.readScreen(m_vframe1->data[0], &fw, &fh, false);
        if (m_vpts == 40) {
            av::save_frame(m_vframe1, "test_frame.ppm");
        }
        // reformat to codec format
        err = sws_scale_frame(m_sws, m_vframe2, m_vframe1);
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

    void ffm_encoder::push_audio(void* samples, size_t len) {
        if (!m_acodec)
            return;
        int err, ilen;
        int16_t* samples_u16;
        // preprocess samples to avoid distortion
        ilen        = len / 4;
        samples_u16 = reinterpret_cast<int16_t*>(samples);
        av::alloc_audio_frame(m_aframe1, ilen, chl_stereo, AV_SAMPLE_FMT_S16);
        std::transform(
            samples_u16, samples_u16 + ilen * 2,
            reinterpret_cast<int16_t*>(m_aframe1->data[0]),
            [](int16_t x) { return (x - 16384) / 4; }
        );

        // push samples into resampler
        swr_convert(
            m_swr, nullptr, 0,
            const_cast<const uint8_t**>(m_aframe1->data),
            ilen
        );
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
        }

        if (m_acodec) {
            // drain last bytes from resampler
            nb_samples = swr_get_out_samples(m_swr, 0);
            av::alloc_audio_frame(
                m_aframe2, nb_samples, m_acodec_ctx->ch_layout,
                m_acodec_ctx->sample_fmt
            );
            err = swr_convert(m_swr, m_aframe2->data, nb_samples, nullptr, 0);
            if (err < 0)
                throw av::av_error(err);

            m_aframe2->pts = m_apts;
            m_apts += m_aframe2->nb_samples;

            av::encode_and_write(
                m_aframe2, m_apacket, m_acodec_ctx, m_astream, m_fmt_ctx
            );
            // flush packets
            av::encode_and_write(
                nullptr, m_apacket, m_acodec_ctx, m_astream, m_fmt_ctx
            );
        }

        // write the file trailer
        if ((err = av_write_trailer(m_fmt_ctx)) < 0)
            throw av::av_error(err);
    }

}  // namespace m64p

struct encoder_backend_interface g_iffmpeg_encoder_backend {
    // init
    [](void** self_, const char* path, m64p_encoder_format fmt) -> m64p_error {
        try {
            m64p::ffm_encoder* self = new m64p::ffm_encoder(path, fmt);
            *self_                  = self;
            return M64ERR_SUCCESS;
        }
        catch (const std::bad_alloc&) {
            return M64ERR_NO_MEMORY;
        }
        catch (const std::system_error&) {
            return M64ERR_SYSTEM_FAIL;
        }
        catch (const m64p::unsupported_error&) {
            return M64ERR_UNSUPPORTED;
        }
        catch (...) {
            return M64ERR_INTERNAL;
        }
    },
        // push_video
        [](void* self_) -> m64p_error {
            try {
                m64p::ffm_encoder* self =
                    static_cast<m64p::ffm_encoder*>(self_);
                self->push_video();
                return M64ERR_SUCCESS;
            }
            catch (const std::bad_alloc& e) {
                return M64ERR_NO_MEMORY;
            }
            catch (const std::system_error& e) {
                return M64ERR_SYSTEM_FAIL;
            }
            catch (const m64p::unsupported_error& e) {
                return M64ERR_UNSUPPORTED;
            }
            catch (...) {
                return M64ERR_INTERNAL;
            }
        },
        // set_sample_rate
        [](void* self_, unsigned int rate) -> m64p_error {
            try {
                m64p::ffm_encoder* self =
                    static_cast<m64p::ffm_encoder*>(self_);
                self->set_sample_rate(rate);
                return M64ERR_SUCCESS;
            }
            catch (const std::bad_alloc& e) {
                return M64ERR_NO_MEMORY;
            }
            catch (const std::system_error& e) {
                return M64ERR_SYSTEM_FAIL;
            }
            catch (const m64p::unsupported_error& e) {
                return M64ERR_UNSUPPORTED;
            }
            catch (...) {
                return M64ERR_INTERNAL;
            }
        },
        // push_audio
        [](void* self_, void* samples, size_t len) -> m64p_error {
            try {
                m64p::ffm_encoder* self =
                    static_cast<m64p::ffm_encoder*>(self_);
                self->push_audio(samples, len);
                return M64ERR_SUCCESS;
            }
            catch (const std::bad_alloc& e) {
                return M64ERR_NO_MEMORY;
            }
            catch (const std::system_error& e) {
                return M64ERR_SYSTEM_FAIL;
            }
            catch (const m64p::unsupported_error& e) {
                return M64ERR_UNSUPPORTED;
            }
            catch (...) {
                return M64ERR_INTERNAL;
            }
        },
        // free
        [](void* self_, bool discard) -> m64p_error {
            try {
                if (self_ == NULL)
                    return M64ERR_INPUT_ASSERT;
                m64p::ffm_encoder* self =
                    static_cast<m64p::ffm_encoder*>(self_);
                self->finish(discard);
                delete self;
                return M64ERR_SUCCESS;
            }
            catch (const std::bad_alloc& e) {
                return M64ERR_NO_MEMORY;
            }
            catch (const std::system_error& e) {
                return M64ERR_SYSTEM_FAIL;
            }
            catch (const m64p::unsupported_error& e) {
                return M64ERR_UNSUPPORTED;
            }
            catch (...) {
                return M64ERR_INTERNAL;
            }
        },
};