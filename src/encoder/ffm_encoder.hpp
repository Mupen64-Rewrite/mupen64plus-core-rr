#ifndef M64P_ENCODER_FFM_ENCODER_HPP
#define M64P_ENCODER_FFM_ENCODER_HPP

#include "avcpp/codec.h"
#include "avcpp/codeccontext.h"
#include "avcpp/formatcontext.h"
#include "backends/api/encoder.h"
extern "C" {
    #include "api/m64p_types.h"
}

namespace m64p {
    class ffm_encoder {
    public:
        ffm_encoder(const char* path, m64p_encoder_format fmt);
        
        m64p_error push_video();
        
        m64p_error set_sample_rate(unsigned int rate);
        
        m64p_error push_audio(void* samples, size_t len);
        
        bool should_discard;
    private:
        av::FormatContext m_fmt_ctx;
        
        av::Stream m_vstream;
        av::VideoEncoderContext m_vcodec_ctx;
        av::Codec m_vcodec;
        av::Packet m_vpacket;
        
        av::Stream m_astream;
        av::AudioEncoderContext m_acodec_ctx;
        av::Codec m_acodec;
        av::Packet m_apacket;
        av::AudioSamples m_aframe;
    };
}

extern "C" struct encoder_backend_interface g_iffmpeg_encoder_backend;

#endif