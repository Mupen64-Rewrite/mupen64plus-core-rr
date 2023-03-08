#include "encoder.h"
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <unistd.h>
#include <future>
#include <system_error>
#include <thread>
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
static std::mutex* enc_mutex;

extern struct encoder_backend_interface g_iffmpeg_encoder_backend;

bool Encoder_IsActive() {
    return ffm_encoder != NULL;
}

EXPORT m64p_error CALL
Encoder_Start(const char* path, m64p_encoder_format format) {
    std::lock_guard _lock(*enc_mutex);
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
}

EXPORT m64p_error CALL Encoder_Stop(bool discard) {
    std::lock_guard _lock(*enc_mutex);
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
}

extern "C" void encoder_startup() {
    enc_mutex = new std::mutex();
}

extern "C" void encoder_shutdown() {
    enc_mutex->lock();
    if (ffm_encoder)
        Encoder_Stop(true);
    enc_mutex->unlock();
    delete enc_mutex;
    enc_mutex = nullptr;
}

extern "C" m64p_error encoder_push_video() {
    std::lock_guard _lock(*enc_mutex);
    if (!ffm_encoder)
        return M64ERR_NOT_INIT;
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
    catch (...) {
        return M64ERR_INTERNAL;
    }
    return M64ERR_SUCCESS;
}
extern "C" m64p_error encoder_set_sample_rate(unsigned int rate) {
    std::lock_guard _lock(*enc_mutex);
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
    catch (...) {
        return M64ERR_INTERNAL;
    }
    return M64ERR_SUCCESS;
}
extern "C" m64p_error encoder_push_audio(const void* data, size_t len) {
    std::lock_guard _lock(*enc_mutex);
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
    catch (...) {
        return M64ERR_INTERNAL;
    }
    return M64ERR_SUCCESS;
}