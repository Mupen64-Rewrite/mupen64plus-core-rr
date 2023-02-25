#ifndef M64P_ENCODER_FFM_ENCODER_HPP
#define M64P_ENCODER_FFM_ENCODER_HPP

extern "C" {
    #include "api/m64p_types.h"
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
}

namespace m64p {
    class ffm_encoder {
    public:
        ffm_encoder(const char* path, m64p_encoder_format fmt);
        
        ~ffm_encoder() {
            avformat_free_context(m_fmt_ctx);
        }
        
        m64p_error push_video();
        
        m64p_error set_sample_rate(unsigned int rate);
        
        m64p_error push_audio(void* samples, size_t len);
        
        bool should_discard;
    private:
        AVFormatContext* m_fmt_ctx;
        
        AVStream* m_vstream;
        AVCodecContext* m_vcodec_ctx;
        const AVCodec* m_vcodec;
        AVPacket* m_vpacket;
        AVFrame* m_vframe;
        
        AVStream* m_astream;
        AVCodecContext* m_acodec_ctx;
        const AVCodec* m_acodec;
        AVPacket* m_apacket;
        AVFrame* m_aframe;
        
        void video_init();
        void audio_init();
    };
}

extern "C" struct encoder_backend_interface g_iffmpeg_encoder_backend;

#endif