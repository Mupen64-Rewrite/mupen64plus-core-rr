#include "ffm_encoder.hpp"
#include <array>
#include <stdexcept>
#include <string>
#include "api/m64p_types.h"
#include "avcpp/codec.h"
#include "avcpp/codeccontext.h"
#include "avcpp/dictionary.h"
#include "avcpp/format.h"
#include "avcpp/pixelformat.h"

using namespace std::literals;

static std::array<std::string_view, 2> fmt_mime_types {
    "video/mp4"sv, "video/webm"sv};

namespace m64p {
    ffm_encoder::ffm_encoder(const char* path, m64p_encoder_format fmt) {
        av::OutputFormat ofmt = (fmt == M64FMT_NULL)
            ? av::OutputFormat(""s, path)
            : av::OutputFormat(""s, ""s, std::string(fmt_mime_types[fmt]));

        enum AVCodecID vcodec_id = ofmt.defaultVideoCodecId();
        enum AVCodecID acodec_id = ofmt.defaultAudioCodecId();

        if (vcodec_id == AV_CODEC_ID_NONE || acodec_id == AV_CODEC_ID_NONE) {
            throw std::invalid_argument("Format does not support video AND audio!");
        }
        
        m_vcodec = av::findEncodingCodec(vcodec_id);
        m_acodec = av::findEncodingCodec(acodec_id);
        
        m_fmt_ctx.setFormat(ofmt);
        
        m_vstream = m_fmt_ctx.addStream(m_vcodec);
        m_astream = m_fmt_ctx.addStream(m_acodec);
        
        m_vcodec_ctx.setWidth(640);
        m_vcodec_ctx.setHeight(480);
        m_vcodec_ctx.setPixelFormat(m_vcodec.supportedPixelFormats()[0]);
        m_vcodec_ctx.setTimeBase({1, 60});
        m_vstream.setTimeBase({1, 60});
        m_vstream.setFrameRate({1, 1});
        
        m_vstream.setAverageFrameRate({1, 1});
    }
}  // namespace m64p