#ifndef M64P_BACKENDS_ENCODER_FFMPEG_H
#define M64P_BACKENDS_ENCODER_FFMPEG_H

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include "api/m64p_types.h"


AVDictionary* load_ffm_options();

// Maintains most encoder state.
typedef struct {
    // The data stream being written to.
    AVStream* stream;
    // Info about the codec.
    const AVCodec* cdinfo;
    // The encoder context.
    AVCodecContext* enc;
    // Packet of encoded data.
    AVPacket* pckt;
    // Frame of unencoded data.
    AVFrame* frame;
    // Internal flags, depending on type.
    uint32_t flags;
    
    union {
        struct SwsContext* sws;
        struct SwrContext* swr;
    };
} ffm_stream;

/**
 * Inits an ffm_stream using an AVFormatContext and an AVCodecID.
 */
m64p_error ffm_stream_init(ffm_stream* stream, AVFormatContext* fmt_ctx, const AVDictionary* opts, enum AVCodecID cdid);
/**
 * Frees an ffm_stream.
 */
void ffm_stream_free(ffm_stream* out);

m64p_error ffm_stream_audio_push(ffm_stream* stream, const void* data, size_t len);
m64p_error ffm_stream_audio_sample_rate(ffm_stream* stream, uint32_t rate);

m64p_error ffm_stream_video_push(ffm_stream* stream, const void* data, size_t width, size_t height);

typedef struct {
    char* path;
    AVFormatContext* fmt_ctx;

    ffm_stream audio;
    ffm_stream video;
} ffm_backend;

#endif