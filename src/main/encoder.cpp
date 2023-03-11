#include "encoder.h"
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <unistd.h>
#include <any>
#include <future>
#include <shared_mutex>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <unordered_map>
#include "encoder/ffm_encoder.hpp"
#define M64P_CORE_PROTOTYPES
#include <mutex>
extern "C" {
#include "api/callbacks.h"
#include "api/m64p_common.h"
#include "api/m64p_encoder.h"
#include "api/m64p_types.h"
}

static m64p::ffm_encoder* ffm_encoder;
// shared_mutex is really a read/write lock.
// - the read lock is the shared lock
// - the write lock is the exclusive lock
static std::shared_mutex enc_rwlock;

static std::unordered_map<std::string, std::string> format_opts;

extern struct encoder_backend_interface g_iffmpeg_encoder_backend;

bool Encoder_IsActive() {
    return ffm_encoder != NULL;
}

EXPORT m64p_error CALL
Encoder_Start(const char* path, m64p_encoder_format format) {
    std::unique_lock _lock(enc_rwlock);
    if (ffm_encoder)
        return M64ERR_ALREADY_INIT;

    try {
        ffm_encoder = new m64p::ffm_encoder(path, format);
        return M64ERR_SUCCESS;
    }
    catch (const m64p::unsupported_error&) {
        return M64ERR_UNSUPPORTED;
    }
    catch (const std::system_error& err) {
        DebugMessage(M64MSG_ERROR, "FFmpeg error: %s", err.what());
        return M64ERR_SYSTEM_FAIL;
    }
    catch (...) {
        return M64ERR_INTERNAL;
    }

    return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL Encoder_Stop(bool discard) {
    std::unique_lock _lock(enc_rwlock);
    if (!ffm_encoder)
        return M64ERR_NOT_INIT;

    try {
        ffm_encoder->finish(discard);
        delete ffm_encoder;
        ffm_encoder = nullptr;
        return M64ERR_SUCCESS;
    }
    catch (const m64p::unsupported_error&) {
        return M64ERR_UNSUPPORTED;
    }
    catch (const std::system_error& err) {
        DebugMessage(M64MSG_ERROR, "FFmpeg error: %s", err.what());
        return M64ERR_SYSTEM_FAIL;
    }
    catch (...) {
        return M64ERR_INTERNAL;
    }

    return M64ERR_SUCCESS;
}

extern "C" void encoder_startup() {}

extern "C" void encoder_shutdown() {
    std::unique_lock _lock(enc_rwlock);
    if (!ffm_encoder)
        return;

    try {
        ffm_encoder->finish(true);
        delete ffm_encoder;
        ffm_encoder = nullptr;
    }
    catch (...) {
    }
}

extern "C" m64p_error encoder_push_video() {
    {
        std::shared_lock _lock(enc_rwlock);
        if (!ffm_encoder)
            return M64ERR_NOT_INIT;
        ffm_encoder->read_screen();

        (void) std::async([=] {
            try {
                ffm_encoder->push_video();
                return M64ERR_SUCCESS;
            }
            catch (const m64p::unsupported_error&) {
                return M64ERR_UNSUPPORTED;
            }
            catch (const std::system_error& err) {
                DebugMessage(M64MSG_ERROR, "FFmpeg error: %s", err.what());
                return M64ERR_SYSTEM_FAIL;
            }
            catch (const std::invalid_argument& err) {
                DebugMessage(M64MSG_ERROR, "FFmpeg error: %s", err.what());
            }
            catch (...) {
                return M64ERR_INTERNAL;
            }
        return M64ERR_INTERNAL;
        });
    }
    return M64ERR_SUCCESS;
}
extern "C" m64p_error encoder_set_sample_rate(unsigned int rate) {
    (void) std::async([=] {
        std::shared_lock _lock(enc_rwlock);
        if (!ffm_encoder)
            return M64ERR_NOT_INIT;
        try {
            ffm_encoder->set_sample_rate(rate);
            return M64ERR_SUCCESS;
        }
        catch (const m64p::unsupported_error&) {
            return M64ERR_UNSUPPORTED;
        }
        catch (const std::system_error& err) {
            DebugMessage(M64MSG_ERROR, "FFmpeg error: %s", err.what());
            return M64ERR_SYSTEM_FAIL;
        }
        catch (const std::invalid_argument& err) {
            DebugMessage(M64MSG_ERROR, "FFmpeg error: %s", err.what());
        }
        catch (...) {
            return M64ERR_INTERNAL;
        }
        return M64ERR_INTERNAL;
    });
    return M64ERR_SUCCESS;
}
extern "C" m64p_error encoder_push_audio(const void* data, size_t len) {
    (void) std::async([=] {
        std::shared_lock _lock(enc_rwlock);
        if (!ffm_encoder)
            return M64ERR_NOT_INIT;
        try {
            ffm_encoder->push_audio(data, len);
            return M64ERR_SUCCESS;
        }
        catch (const m64p::unsupported_error&) {
            return M64ERR_UNSUPPORTED;
        }
        catch (const std::system_error& err) {
            DebugMessage(M64MSG_ERROR, "FFmpeg error: %s", err.what());
            return M64ERR_SYSTEM_FAIL;
        }
        catch (const std::invalid_argument& err) {
            DebugMessage(M64MSG_ERROR, "FFmpeg error: %s", err.what());
        }
        catch (...) {
            return M64ERR_INTERNAL;
        }
        return M64ERR_INTERNAL;
    });
    return M64ERR_SUCCESS;
}