#include <libavcodec/codec.h>
#include "api/m64p_types.h"
#include "api/callbacks.h"
#include "ffmpeg.h"

static m64p_error ffm_stream_audio_init(ffm_stream* out, enum AVCodecID cdid);

m64p_error ffm_stream_init(
    ffm_stream* out, AVFormatContext* fmt_ctx, enum AVCodecID cdid
) {
    const AVCodec* info;
    AVCodecContext* enc;
    
    // clear all fields first
    memset(out, 0, sizeof(ffm_stream));
    
    // Find codec
    info = avcodec_find_encoder(cdid);
    if (!info) {
        DebugMessage(M64MSG_ERROR, "FFmpeg: No available encoder for %s", avcodec_get_name(cdid));
        return M64ERR_SYSTEM_FAIL;
    }
    out->cdinfo = info;
    
    // allocate temp packet, stream, and encoder
    out->pckt = av_packet_alloc();
    if (!out->pckt) {
        DebugMessage(M64MSG_ERROR, "FFmpeg: Could not alloc AVPacket");
        return M64ERR_SYSTEM_FAIL;
    }
    out->stream = avformat_new_stream(fmt_ctx, info);
    if (!out->stream) {
        DebugMessage(M64MSG_ERROR, "FFmpeg: Could not alloc AVStream");
        return M64ERR_SYSTEM_FAIL;
    }
    out->stream->id = fmt_ctx->nb_streams - 1;
    enc = avcodec_alloc_context3(info);
    if (!enc) {
        DebugMessage(M64MSG_ERROR, "FFmpeg: Could not alloc AVCodecContext");
        return M64ERR_SYSTEM_FAIL;
    }
    out->enc = enc;
    
    
}