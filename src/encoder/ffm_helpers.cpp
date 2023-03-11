#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/opt.h>
#include "api/callbacks.h"
#include "api/m64p_types.h"
#define M64P_CORE_PROTOTYPES 1

#include "ffm_helpers.hpp"

extern "C" {
#include "api/m64p_config.h"
}

namespace {
}  // namespace

namespace av {
    AVDictionary* config_to_dict(m64p_handle handle) {
        int err;
        struct _cb1_ctx {
            m64p_handle cfg_handle;
            AVDictionary* dict;
        } ctx {handle, nullptr};

        err = ConfigListParameters(handle, &ctx, [](void* ctx_, const char* name, m64p_type) {
            int err;
            _cb1_ctx* ctx = static_cast<_cb1_ctx*>(ctx_);
            
            const char* value = ConfigGetParamString(ctx->cfg_handle, name);
            if ((err = av_dict_set(&ctx->dict, name, value, 0)) < 0) {
                char errmsg[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(err, errmsg, sizeof(errmsg));
                DebugMessage(M64MSG_WARNING, "FFmpeg: failed to set dict value: %s", errmsg);
            }
        });

        if (err != M64ERR_SUCCESS) {
            DebugMessage(M64MSG_ERROR, "setting config options failed");
            return nullptr;
        }
        return ctx.dict;
    }
}  // namespace m64p
