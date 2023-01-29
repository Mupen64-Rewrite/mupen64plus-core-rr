#ifndef M64P_BACKENDS_ENCODER_FFMPEG_H
#define M64P_BACKENDS_ENCODER_FFMPEG_H

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include "api/m64p_types.h"

// Maintains most encoder state.
typedef struct {
    // The data stream being written to.
    AVStream* stream;
    // Info about the codec.
    const AVCodec* cdinfo;
    // The encoder context.
    AVCodecContext* enc;
    // A temporary packet to encode data.
    AVPacket* pckt;
    // A frame to hold raw data.
    AVFrame* in_frame;
    // A frame holding data to be encoded.
    AVFrame* out_frame;
    
    union {
        struct SwsContext* sws;
        struct SwrContext* swr;
    };
} ffm_stream;

/**
 * Init an ffm_stream using an AVFormatContext and an AVCodecID.
 */
m64p_error ffm_stream_init(ffm_stream* stream, AVFormatContext* fmt_ctx, enum AVCodecID cdid);

typedef struct {
    char* path;
    AVFormatContext* fmt_ctx;

    ffm_stream audio;
    ffm_stream video;
} ffm_backend;

#endif