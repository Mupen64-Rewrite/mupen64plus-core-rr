#include "ffm_encoder.hpp"
#include <libavcodec/avcodec.h>
#include <libavcodec/codec.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <array>
#include <new>
#include <stdexcept>
#include <string>
#include <system_error>
#include "api/m64p_types.h"
#include "backends/api/encoder.h"
#include "encoder/ffm_error.hpp"

using namespace std::literals;

static const char* fmt_mime_types[] = {"video/mp4", "video/webm"};

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
            ) < 0) {
            throw av::av_error(err);
        }

        // find codecs
        m_vcodec = (ofmt->video_codec != AV_CODEC_ID_NONE)
            ? avcodec_find_encoder(ofmt->video_codec)
            : nullptr;
        m_acodec = (ofmt->audio_codec != AV_CODEC_ID_NONE)
            ? avcodec_find_encoder(ofmt->audio_codec)
            : nullptr;

        // alloc and init video stream
        if (m_vcodec) {
            m_vstream = avformat_new_stream(m_fmt_ctx, m_vcodec);
            m_vcodec_ctx = avcodec_alloc_context3(m_vcodec);
            m_vpacket = av_packet_alloc();
            m_vframe = av_frame_alloc();
            
            if (!m_vstream || !m_vcodec_ctx || !m_vpacket || !m_vframe) {
                throw std::bad_alloc();
            }
            video_init();
        }
        else {
            m_vstream = nullptr;
            m_vcodec_ctx = nullptr;
            m_vpacket = nullptr;
            m_vframe = nullptr;
        }
        
        // alloc and init audio stream
        if (m_acodec) {
            m_astream = avformat_new_stream(m_fmt_ctx, m_acodec);
            m_acodec_ctx = avcodec_alloc_context3(m_acodec);
            m_apacket = av_packet_alloc();
            m_aframe = av_frame_alloc();
            
            if (!m_astream || !m_acodec_ctx || !m_apacket || !m_aframe) {
                throw std::bad_alloc();
            }
            audio_init();
        }
        else {
            m_astream = nullptr;
            m_acodec_ctx = nullptr;
            m_apacket = nullptr;
            m_aframe = nullptr;
        }

    }
    
    void ffm_encoder::video_init() {
        
    }
}  // namespace m64p

struct encoder_backend_interface g_iffmpeg_encoder_backend {
    // init
    [](void** self_, const char* path, m64p_encoder_format fmt) -> m64p_error {
        try {
            m64p::ffm_encoder* self = new m64p::ffm_encoder(path, fmt);
            *self_ = self;
            return M64ERR_SUCCESS;
        }
        catch (const std::bad_alloc&) {
            return M64ERR_NO_MEMORY;
        }
        catch (const std::system_error&) {
            return M64ERR_SYSTEM_FAIL;
        }
        catch (...) {
            return M64ERR_INTERNAL;
        }
    },
    // push_video
    [](void* self_) -> m64p_error {
        try {
            m64p::ffm_encoder* self = static_cast<m64p::ffm_encoder*>(self_);
            self->push_video();
            return M64ERR_SUCCESS;
        }
        catch (const std::bad_alloc&) {
            return M64ERR_NO_MEMORY;
        }
        catch (const std::system_error&) {
            return M64ERR_SYSTEM_FAIL;
        }
        catch (...) {
            return M64ERR_INTERNAL;
        }
    },
    // set_sample_rate
    [](void* self_, unsigned int rate) -> m64p_error {
        try {
            m64p::ffm_encoder* self = static_cast<m64p::ffm_encoder*>(self_);
            self->set_sample_rate(rate);
            return M64ERR_SUCCESS;
        }
        catch (const std::bad_alloc&) {
            return M64ERR_NO_MEMORY;
        }
        catch (const std::system_error&) {
            return M64ERR_SYSTEM_FAIL;
        }
        catch (...) {
            return M64ERR_INTERNAL;
        }
    },
    // push_audio
    [](void* self_, void* samples, size_t len) -> m64p_error {
        try {
            m64p::ffm_encoder* self = static_cast<m64p::ffm_encoder*>(self_);
            self->push_audio(samples, len);
            return M64ERR_SUCCESS;
        }
        catch (const std::bad_alloc&) {
            return M64ERR_NO_MEMORY;
        }
        catch (const std::system_error&) {
            return M64ERR_SYSTEM_FAIL;
        }
        catch (...) {
            return M64ERR_INTERNAL;
        }
    },
    // free
    [](void* self_, bool discard) -> void {
        try {
            m64p::ffm_encoder* self = static_cast<m64p::ffm_encoder*>(self_);
            delete self;
        }
        catch (const std::exception&) {
        }
    },
};