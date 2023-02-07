#include <libavcodec/avcodec.h>
#include <libavcodec/codec.h>
#include <libavutil/avutil.h>
#include <libavutil/dict.h>
#include <libavutil/frame.h>
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <string.h>
#include <stdbool.h>
#include "api/m64p_types.h"
#include "api/callbacks.h"
#include "ffmpeg.h"

static inline int abs_diff(int x, int y) {
    return (x >= y)? x - y : y - x;
}

typedef enum {
    FFMSF_AUDIO_VARIABLE = (1 << 0),
} ffm_stream_flags;

static m64p_error audio_stream_init(ffm_stream* self, AVDictionary* opts);
static m64p_error video_stream_init(ffm_stream* self, AVDictionary* opts);

m64p_error ffm_stream_init(
    ffm_stream* self, AVFormatContext* fmt_ctx, const AVDictionary* opts, enum AVCodecID cdid
) {
    const AVCodec* info;
    AVCodecContext* enc;
    
    // clear all fields first
    memset(self, 0, sizeof(ffm_stream));
    
    // Find codec
    info = avcodec_find_encoder(cdid);
    if (!info) {
        DebugMessage(M64MSG_ERROR, "FFmpeg: No available encoder for %s", avcodec_get_name(cdid));
        ffm_stream_free(self);
        return M64ERR_SYSTEM_FAIL;
    }
    self->cdinfo = info;
    
    // allocate temp packet, stream, and encoder
    self->pckt = av_packet_alloc();
    if (!self->pckt) {
        DebugMessage(M64MSG_ERROR, "FFmpeg: Could not alloc AVPacket");
        ffm_stream_free(self);
        return M64ERR_SYSTEM_FAIL;
    }
    self->frame = av_frame_alloc();
    if (!self->frame) {
        DebugMessage(M64MSG_ERROR, "FFmpeg: Could not alloc AVFrame");
        ffm_stream_free(self);
    }
    self->stream = avformat_new_stream(fmt_ctx, info);
    if (!self->stream) {
        DebugMessage(M64MSG_ERROR, "FFmpeg: Could not alloc AVStream");
        ffm_stream_free(self);
        return M64ERR_SYSTEM_FAIL;
    }
    self->stream->id = fmt_ctx->nb_streams - 1;
    enc = avcodec_alloc_context3(info);
    if (!enc) {
        DebugMessage(M64MSG_ERROR, "FFmpeg: Could not alloc AVCodecContext");
        ffm_stream_free(self);
        return M64ERR_SYSTEM_FAIL;
    }
    self->enc = enc;
    
    // copy options dict
    AVDictionary* opts2 = NULL;
    av_dict_copy(&opts2, opts, 0);
    
    switch (info->type) {
    case AVMEDIA_TYPE_AUDIO:
        return audio_stream_init(self, opts2);
    case AVMEDIA_TYPE_VIDEO:
        return video_stream_init(self, opts2);
    }
}

static m64p_error audio_stream_init(ffm_stream* self, AVDictionary* opts) {
    int err;
    AVCodecContext* enc = self->enc;
    const AVCodec* info = self->cdinfo;
    AVFrame* f = self->frame;
    
    // set all defaults first
    av_opt_set_defaults(enc);
    
    // find the best sample rate
    if (info->supported_samplerates != NULL) {
        int best_rate = info->supported_samplerates[0];
        for (const int* i = info->supported_samplerates + 1; *i != 0; i++) {
            if (abs_diff(*i, 48000) < abs_diff(best_rate, 48000)) {
                best_rate = *i;
            }
        }
        enc->sample_rate = best_rate;
    }
    else {
        enc->sample_rate = 48000;
    }
    
    // try to use 16-bit samples
    if (info->sample_fmts != NULL) {
        int best_fmt = info->sample_fmts[0];
        for (const enum AVSampleFormat* i = info->sample_fmts; *i != -1; i++) {
            if (*i == AV_SAMPLE_FMT_S16) {
                best_fmt = AV_SAMPLE_FMT_S16;
                break;
            }
            else if (*i == AV_SAMPLE_FMT_S16P) {
                best_fmt = AV_SAMPLE_FMT_S16P;
            }
        }
        enc->sample_fmt = best_fmt;
    }
    else {
        enc->sample_fmt = AV_SAMPLE_FMT_S16;
    }
    
    // try to use stereo
    if (info->ch_layouts != NULL) {
        const AVChannelLayout* best_layout = &info->ch_layouts[0];
        for (const AVChannelLayout* i = info->ch_layouts; i->nb_channels != 0; i++) {
            if (i->u.mask == AV_CH_LAYOUT_STEREO) {
                best_layout = i;
                break;
            }
        }
        av_channel_layout_copy(&enc->ch_layout, best_layout);
    }
    else {
        av_channel_layout_copy(&enc->ch_layout, &(AVChannelLayout) AV_CHANNEL_LAYOUT_STEREO);
    }
    
    // open codec with options
    err = avcodec_open2(enc, info, &opts);
    if (err < 0) {
        DebugMessage(M64MSG_ERROR, "FFmpeg: Could not open codec: %s", av_err2str(err));
        ffm_stream_free(self);
        return M64ERR_SYSTEM_FAIL;
    }
    av_dict_free(&opts);
    
    // Setup output frame for codec
    f->format = enc->sample_fmt;
    f->sample_rate = enc->sample_rate;
    av_channel_layout_copy(&f->ch_layout, &enc->ch_layout);
    f->nb_samples = enc->frame_size;
    
    // setup swresample context
    err = swr_alloc_set_opts2(&self->swr, 
        // output channel layout, sample format, sample rate
        &enc->ch_layout, enc->sample_fmt, enc->sample_rate, 
        // input channel layout, sample format, sample rate
        &(AVChannelLayout) AV_CHANNEL_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, 44100, 
        0, NULL
    );
    if (err < 0) {
        DebugMessage(M64MSG_ERROR, "FFmpeg: Could not setup resampler: %s", av_err2str(err));
        ffm_stream_free(self);
        return M64ERR_SYSTEM_FAIL;
    }
    
    return M64ERR_SUCCESS;
}

static m64p_error video_stream_init(ffm_stream* self, AVDictionary* opts) {
    int err;
    AVDictionaryEntry* ent;
    
    AVCodecContext* enc = self->enc;
    const AVCodec* info = self->cdinfo;
    AVFrame* f = self->frame;
    
    // set all defaults first
    av_opt_set_defaults(enc);
    
    // frame group size
    enc->gop_size = 30;
    // pixel format, just use the first one, because most video codecs don't use RGB
    enc->pix_fmt = info->pix_fmts[0];
    
    if ((ent = av_dict_get(opts, "video_size", NULL, 0)) == NULL) {
        av_opt_set_image_size(enc, "video_size", 640, 480, 0);
    }
    
    // open codec with options
    err = avcodec_open2(enc, info, &opts);
    if (err < 0) {
        DebugMessage(M64MSG_ERROR, "FFmpeg: Could not open codec: %s", av_err2str(err));
        ffm_stream_free(self);
        return M64ERR_SYSTEM_FAIL;
    }
    av_dict_free(&opts);
    
    // Setup output frame for codec
    f->format = enc->pix_fmt;
    f->width = enc->width;
    f->height = enc->height;
    
    // setup sws context
    self->sws = sws_getContext(
        // input size/format
        640, 480, AV_PIX_FMT_RGB24, 
        // output size/format
        f->width, f->height, f->format, 
        // filter options
        0, NULL, NULL, NULL
    );
    if (self->sws == NULL) {
        DebugMessage(M64MSG_ERROR, "FFmpeg: Could not setup rescaler");
        ffm_stream_free(self);
        return M64ERR_SYSTEM_FAIL;
    }
    return M64ERR_SUCCESS;
}

void ffm_stream_free(ffm_stream* self) {
    if (self->enc != NULL)
        avcodec_free_context(&self->enc);
    if (self->pckt != NULL)
        av_packet_free(&self->pckt);
    if (self->frame != NULL)
        av_frame_free(&self->frame);
    // Free type-specific resources
    switch (self->cdinfo->type) {
    case AVMEDIA_TYPE_AUDIO: {
        if (self->swr != NULL)
            swr_free(&self->swr);
        return;
    }
    case AVMEDIA_TYPE_VIDEO: {
        if (self->sws != NULL) {
            sws_freeContext(self->sws);
            self->sws = NULL;
        }
        return;
    }
    default:
        return;
    }
}