#include "encoder.h"
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <unistd.h>
#include <system_error>
#include <thread>
#include "backends/encoder/ffm_encoder.hpp"
#define M64P_CORE_PROTOTYPES
#include <SDL2/SDL_mutex.h>
extern "C" {
#include "api/callbacks.h"
#include "api/m64p_common.h"
#include "api/m64p_encoder.h"
#include "api/m64p_types.h"
}

static m64p::ffm_encoder* ffm_encoder;
static struct SDL_mutex* enc_mutex;

extern struct encoder_backend_interface g_iffmpeg_encoder_backend;

bool Encoder_IsActive() {
    return ffm_encoder != NULL;
}

EXPORT m64p_error CALL
Encoder_Start(const char* path, m64p_encoder_format format) {
    m64p_error res = M64ERR_INTERNAL;
    SDL_LockMutex(enc_mutex);

    try {
        if (ffm_encoder != NULL)
            return M64ERR_ALREADY_INIT;
        ffm_encoder = new m64p::ffm_encoder(path, format);
        res         = M64ERR_SUCCESS;
    }
    catch (const m64p::unsupported_error&) {
        res = M64ERR_UNSUPPORTED;
    }
    catch (const std::system_error&) {
        res = M64ERR_SYSTEM_FAIL;
    }
    catch (...) {
    }

    SDL_UnlockMutex(enc_mutex);
    return res;
}

EXPORT m64p_error CALL Encoder_Stop(bool discard) {
    m64p_error res = M64ERR_INTERNAL;
    SDL_LockMutex(enc_mutex);

    try {
        if (ffm_encoder == NULL)
            return M64ERR_NOT_INIT;
        ffm_encoder->finish(discard);
        delete ffm_encoder;
        ffm_encoder = nullptr;
        res         = M64ERR_SUCCESS;
    }
    catch (const m64p::unsupported_error&) {
        res = M64ERR_UNSUPPORTED;
    }
    catch (const std::system_error&) {
        res = M64ERR_SYSTEM_FAIL;
    }
    catch (...) {
    }

    SDL_UnlockMutex(enc_mutex);
    return res;
}

void encoder_startup() {
    enc_mutex = SDL_CreateMutex();
}
void encoder_shutdown() {
    SDL_UnlockMutex(enc_mutex);
    SDL_DestroyMutex(enc_mutex);
}
m64p_error encoder_push_video() {
    m64p_error res = M64ERR_INTERNAL;
    SDL_LockMutex(enc_mutex);

    try {
        if (ffm_encoder == NULL)
            return M64ERR_NOT_INIT;
        ffm_encoder->push_video();
        res = M64ERR_SUCCESS;
    }
    catch (const m64p::unsupported_error&) {
        res = M64ERR_UNSUPPORTED;
    }
    catch (const std::system_error&) {
        res = M64ERR_SYSTEM_FAIL;
    }
    catch (...) {
    }

    SDL_UnlockMutex(enc_mutex);
    return res;
}
m64p_error encoder_set_sample_rate(unsigned int rate) {
    m64p_error res = M64ERR_INTERNAL;
    SDL_LockMutex(enc_mutex);

    try {
        if (ffm_encoder == NULL)
            return M64ERR_NOT_INIT;
        ffm_encoder->set_sample_rate(rate);
        res = M64ERR_SUCCESS;
    }
    catch (const m64p::unsupported_error&) {
        res = M64ERR_UNSUPPORTED;
    }
    catch (const std::system_error&) {
        res = M64ERR_SYSTEM_FAIL;
    }
    catch (...) {
    }

    SDL_UnlockMutex(enc_mutex);
    return res;
}
m64p_error encoder_push_audio(void* data, size_t len) {
    using namespace std::literals;

    m64p_error res = M64ERR_INTERNAL;
    SDL_LockMutex(enc_mutex);

    try {
        if (ffm_encoder == NULL)
            return M64ERR_NOT_INIT;
        ffm_encoder->push_audio(data, len);
        res = M64ERR_SUCCESS;
    }
    catch (const m64p::unsupported_error&) {
        res = M64ERR_UNSUPPORTED;
    }
    catch (const std::system_error&) {
        res = M64ERR_SYSTEM_FAIL;
    }
    catch (...) {
    }

    SDL_UnlockMutex(enc_mutex);
    return res;
}