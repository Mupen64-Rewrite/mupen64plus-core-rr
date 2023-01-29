#include "ffmpeg.h"
#include <stdbool.h>
#include <string.h>
#include "SDL_stdinc.h"
#include "api/callbacks.h"
#include "api/m64p_types.h"
#include "backend.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

// most code adapted from here:
// https://ffmpeg.org/doxygen/trunk/muxing_8c-example.html



// HELPERS
// ===============================

static void ffmpeg_stream_free(ffm_stream* out) {
    if (out->enc != NULL)
        avcodec_free_context(&out->enc);
    if (out->pckt != NULL)
        av_packet_free(&out->pckt);
    if (out->in_frame != NULL)
        av_frame_free(&out->in_frame);
    if (out->out_frame != NULL)
        av_frame_free(&out->out_frame);
    // Free type-specific resources
    switch (out->cdinfo->type) {
    case AVMEDIA_TYPE_AUDIO: {
        if (out->swr != NULL)
            swr_free(&out->swr);
        return;
    }
    case AVMEDIA_TYPE_VIDEO: {
        if (out->sws != NULL) {
            sws_freeContext(out->sws);
            out->sws = NULL;
        }
        return;
    }
    default:
        return;
    }
}

static bool ffmpeg_stream_init(ffm_stream* out, AVFormatContext* fmt_ctx, enum AVCodecID codec_id) {
    int err;
    const AVCodec* codec;
    AVCodecContext* c;
    
    // find codec
    codec = avcodec_find_encoder(codec_id);
    if (!codec) {
        DebugMessage(M64MSG_ERROR, "FFmpeg: No available encoder for %s", avcodec_get_name(codec_id));
        return false;
    }
    out->cdinfo = codec;
    
    // allocate temp packet, stream, and encoder
    out->pckt = av_packet_alloc();
    if (!out->pckt) {
        DebugMessage(M64MSG_ERROR, "FFmpeg: Could not alloc AVPacket");
        return false;
    }
    out->stream = avformat_new_stream(fmt_ctx, codec);
    if (!out->stream) {
        DebugMessage(M64MSG_ERROR, "FFmpeg: Could not alloc AVStream");
        return false;
    }
    out->stream->id = fmt_ctx->nb_streams - 1;
    c = avcodec_alloc_context3(codec);
    if (!c) {
        DebugMessage(M64MSG_ERROR, "FFmpeg: Could not alloc AVCodecContext");
        return false;
    }
    out->enc = c;
    
    
    return true;
}

static bool ffmpeg_stream_open_audio(ffm_stream* out) {
    int err;
    AVCodecContext* c = out->enc;
    
    err = avcodec_open2(c, out->cdinfo, NULL);
    if (err < 0) {
        DebugMessage(M64MSG_ERROR, "FFmpeg error: %s", av_err2str(err));
        ffmpeg_stream_free(out);
        return false;
    }
}

static bool ffmpeg_stream_open_video(ffm_stream* out) {
    int err;
    AVCodecContext* c = out->enc;
    
    err = avcodec_open2(c, out->cdinfo, NULL);
    if (err < 0) {
        DebugMessage(M64MSG_ERROR, "FFmpeg error: %s", av_err2str(err));
        ffmpeg_stream_free(out);
        return false;
    }
}

static bool ffmpeg_stream_open(ffm_stream* out) {
    if (out->stream == NULL)
        return true;
    switch (out->cdinfo->type) {
    case AVMEDIA_TYPE_AUDIO:
        return ffmpeg_stream_open_audio(out);
    case AVMEDIA_TYPE_VIDEO:
        return ffmpeg_stream_open_video(out);
    default:
        return false;
    }
}

static void ffmpeg_backend_free(ffm_backend* self) {
    SDL_free(self->path);
    avformat_free_context(self->fmt_ctx);
    
    // free myself last
    free(self);
}

// EXPOSED API FUNCTIONS
// ===============================

static const char* m64p_encoder_map[] = {
    "video/mp4", // M64FMT_MP4
    "video/webm" // M64FMT_WEBM
};

m64p_error ffmpeg_backend_init(
    void** self_, const char* path, m64p_encoder_format m64p_fmt
) {
    int err;
    const AVOutputFormat* fmt = NULL;

    // initialize the backend
    ffm_backend* self = malloc(sizeof(ffm_backend));
    memset(self, 0, sizeof(ffm_backend));
    // SDL always provides this, which is nice
    self->path = SDL_strdup(path);
    
    // See if a format was specified; if not, let FFmpeg guess
    if (m64p_fmt >= 0) {
        fmt = av_guess_format(NULL, NULL, m64p_encoder_map[m64p_fmt]);
    }
    
    // init format context
    err = avformat_alloc_output_context2(&self->fmt_ctx, fmt, NULL, path);
    if (err < 0) {
        DebugMessage(M64MSG_ERROR, "FFmpeg error: %s", av_err2str(err));
        ffmpeg_backend_free(self);
        return M64ERR_SYSTEM_FAIL;
    }

    // initialize encoder streams
    fmt = self->fmt_ctx->oformat;
    if (fmt->video_codec != AV_CODEC_ID_NONE) {
        if (!ffmpeg_stream_init(&self->video, self->fmt_ctx, fmt->video_codec)) {
            ffmpeg_backend_free(self);
            return M64ERR_SYSTEM_FAIL;
        }
    }
    if (fmt->audio_codec != AV_CODEC_ID_NONE) {
        if (!ffmpeg_stream_init(&self->audio, self->fmt_ctx, fmt->audio_codec)) {
            ffmpeg_backend_free(self);
            return M64ERR_SYSTEM_FAIL;
        }
    }
}


// const struct encoder_backend_interface g_ffmpeg_encoder_backend = {
//     .init = ffmpeg_backend_init,
// };