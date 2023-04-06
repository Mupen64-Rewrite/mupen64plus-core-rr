

#define M64P_CORE_PROTOTYPES 1

#include "ffm_helpers.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>

#include "api/callbacks.h"
#include "api/m64p_config.h"
#include "api/m64p_plugin.h"
#include "api/m64p_types.h"
}

namespace {
    // This function just uses the default codec suggested by FFmpeg.
    // It may be better to manually select for certain formats.
    const AVCodec* default_audio_codec(const AVOutputFormat* fmt) {
        return avcodec_find_encoder(fmt->audio_codec);
    }
    // This function just uses the default codec suggested by FFmpeg.
    // It may be better to manually select for certain formats.
    const AVCodec* default_video_codec(const AVOutputFormat* fmt) {
        return avcodec_find_encoder(fmt->video_codec);
    }
}  // namespace

namespace av {
    void select_codecs(
        const AVOutputFormat* ofmt, const AVCodec*& vcodec,
        const AVCodec*& acodec
    ) {
        int err;
        m64p_handle cfg_handle;
        m64p_type tmp1;

        const char *acodec_name, *vcodec_name;
        acodec_name = vcodec_name = nullptr;
        acodec = vcodec = nullptr;

        // read relevant config options
        if ((err = ConfigOpenSection("EncFFmpeg-Format", &cfg_handle)) ==
            M64ERR_SUCCESS) {
            err = ConfigGetParameterType(cfg_handle, "audio_codec", &tmp1);
            if (err == M64ERR_SUCCESS) {
                acodec_name = ConfigGetParamString(cfg_handle, "audio_codec");
            }
            err = ConfigGetParameterType(cfg_handle, "video_codec", &tmp1);
            if (err == M64ERR_SUCCESS) {
                vcodec_name = ConfigGetParamString(cfg_handle, "video_codec");
            }
        }

        // Select audio codec based on config
        if (acodec_name != nullptr) {
            acodec = avcodec_find_encoder_by_name(acodec_name);
            if (acodec->type != AVMEDIA_TYPE_AUDIO) {
                DebugMessage(
                    M64MSG_WARNING,
                    "Encoder %s (codec %s) is not an audio codec, using default...",
                    acodec->name, avcodec_get_name(acodec->id)
                );
                acodec = default_audio_codec(ofmt);
            }
            else if (!avformat_query_codec(
                    ofmt, acodec->id, FF_COMPLIANCE_EXPERIMENTAL
                )) {
                DebugMessage(
                    M64MSG_WARNING,
                    "Encoder %s (codec %s) incompatible with format %s, using default...",
                    acodec->name, avcodec_get_name(acodec->id), ofmt->name
                );
                acodec = default_audio_codec(ofmt);
            }
        }
        else {
            acodec = default_audio_codec(ofmt);
        }
        DebugMessage(
            M64MSG_INFO, "Using audio encoder %s (codec %s)", acodec->name,
            avcodec_get_name(acodec->id)
        );

        // Select video codec based on config
        if (vcodec_name != nullptr) {
            vcodec = avcodec_find_encoder_by_name(vcodec_name);
            if (vcodec->type != AVMEDIA_TYPE_VIDEO) {
                DebugMessage(
                    M64MSG_WARNING,
                    "Encoder %s (codec %s) is not a video codec, using default...",
                    vcodec->name, avcodec_get_name(vcodec->id)
                );
                vcodec = default_video_codec(ofmt);
            }
            else if (!avformat_query_codec(
                    ofmt, vcodec->id, FF_COMPLIANCE_EXPERIMENTAL
                )) {
                DebugMessage(
                    M64MSG_WARNING,
                    "Encoder %s (codec %s) incompatible with format %s, using default...",
                    vcodec->name, avcodec_get_name(vcodec->id), ofmt->name
                );
                vcodec = default_video_codec(ofmt);
            }
        }
        else {
            vcodec = default_video_codec(ofmt);
        }
        DebugMessage(
            M64MSG_INFO, "Using video encoder %s (codec %s)", vcodec->name,
            avcodec_get_name(vcodec->id)
        );
    }

    AVDictionary* config_to_dict(m64p_handle handle) {
        int err;
        struct _cb1_ctx {
            m64p_handle cfg_handle;
            AVDictionary* dict;
        } ctx {handle, nullptr};

        err = ConfigListParameters(
            handle, &ctx,
            [](void* ctx_, const char* name, m64p_type) {
                int err;
                _cb1_ctx* ctx = static_cast<_cb1_ctx*>(ctx_);

                const char* value = ConfigGetParamString(ctx->cfg_handle, name);
                if ((err = av_dict_set(&ctx->dict, name, value, 0)) < 0) {
                    char errmsg[AV_ERROR_MAX_STRING_SIZE];
                    av_strerror(err, errmsg, sizeof(errmsg));
                    DebugMessage(
                        M64MSG_WARNING, "FFmpeg: failed to set dict value: %s",
                        errmsg
                    );
                }
            }
        );

        if (err != M64ERR_SUCCESS) {
            DebugMessage(M64MSG_ERROR, "setting config options failed");
            return nullptr;
        }
        return ctx.dict;
    }
}  // namespace av
