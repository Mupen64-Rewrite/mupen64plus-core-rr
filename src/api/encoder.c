#define M64P_CORE_PROTOTYPES
#include "backends/api/encoder.h"
#include "SDL_mutex.h"
#include "api/callbacks.h"
#include "api/m64p_common.h"
#include "api/m64p_types.h"
#include "m64p_encoder.h"
#include "encoder.h"

#include <SDL2/SDL_mutex.h>

static void* backend_obj = NULL;
static struct encoder_backend_interface* ibackend;
static struct SDL_mutex* enc_mutex;

extern struct encoder_backend_interface g_iffmpeg_encoder_backend;

BOOL Encoder_IsActive() {
    return backend_obj != NULL;
}

EXPORT m64p_error CALL Encoder_Start(const char* path, m64p_encoder_format format) {
    m64p_error res;
    SDL_LockMutex(enc_mutex);
    res = ibackend->init(&backend_obj, path, format);
    SDL_UnlockMutex(enc_mutex);
    return res;
}

EXPORT m64p_error CALL Encoder_Stop(int discard) {
    SDL_LockMutex(enc_mutex);
    m64p_error res = ibackend->free(backend_obj, discard);
    backend_obj = NULL;
    SDL_UnlockMutex(enc_mutex);
    return res;
}

void encoder_startup() {
    if (backend_obj)
        return;
    ibackend = &g_iffmpeg_encoder_backend;
    enc_mutex = SDL_CreateMutex();
}
void encoder_shutdown() {
    if (!backend_obj)
        return;
    SDL_LockMutex(enc_mutex);
    if (backend_obj != NULL)
        ibackend->free(backend_obj, false);
    backend_obj = NULL;
    SDL_UnlockMutex(enc_mutex);
    
    SDL_DestroyMutex(enc_mutex);
}
void encoder_push_video() {
    if (!backend_obj)
        return;
    
    m64p_error res;
    SDL_LockMutex(enc_mutex);
    
    res = ibackend->push_video(backend_obj);
    if (res != M64ERR_SUCCESS) {
        DebugMessage(M64MSG_ERROR, "encoder_push_video | %s", CoreErrorMessage(res));
    } 
    SDL_UnlockMutex(enc_mutex);
}
void encoder_set_sample_rate(unsigned int rate) {
    if (!backend_obj)
        return;
    
    m64p_error res;
    SDL_LockMutex(enc_mutex);
    
    res = ibackend->set_sample_rate(backend_obj, rate);
    if (res != M64ERR_SUCCESS) {
        DebugMessage(M64MSG_ERROR, "encoder_set_sample_rate | %s", CoreErrorMessage(res));
    } 
    SDL_UnlockMutex(enc_mutex);
}
void encoder_push_audio(void *data, size_t len) {
    if (!backend_obj)
        return;
    
    m64p_error res;
    SDL_LockMutex(enc_mutex);
    
    res = ibackend->push_audio(backend_obj, data, len);
    if (res != M64ERR_SUCCESS) {
        DebugMessage(M64MSG_ERROR, "encoder_push_audio | %s", CoreErrorMessage(res));
    } 
    SDL_UnlockMutex(enc_mutex);
}