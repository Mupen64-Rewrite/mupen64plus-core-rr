#include "encoder.h"
#include <stdbool.h>
#include <any>
#include <future>
#include <shared_mutex>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <unordered_map>
#define M64P_CORE_PROTOTYPES
#define M64P_ENCODER_PROTOTYPES
#include <mutex>
extern "C" {
#include "api/m64p_encoder.h"
#include "api/m64p_types.h"
#include "main.h"  //for sample rate
}

static bool g_encoder_active = false;
// shared_mutex is really a read/write lock.
// - the read lock is the shared lock
// - the write lock is the exclusive lock
static std::shared_mutex enc_rwlock;

static std::unordered_map<std::string, std::string> format_opts;

// Audio stuff
SampleCallback* g_sample_callback            = NULL;
RateChangedCallback* g_rate_changed_callback = NULL;

EXPORT m64p_error CALL Encoder_SetSampleCallback(SampleCallback* callback) {
    g_sample_callback = callback;
    return M64ERR_SUCCESS;
}
EXPORT m64p_error CALL Encoder_SetRateChangedCallback(RateChangedCallback* callback) {
    g_rate_changed_callback = callback;
    return M64ERR_SUCCESS;
}

EXPORT unsigned int CALL Encoder_GetSampleRate(void) {
    // dacrate = ai->vi->clock / frequency - 1
    // ai->vi->clock/(dacrate+1) = frequency
    return (unsigned int) g_dev.ai.vi->clock /
        (g_dev.ai.regs[AI_DACRATE_REG] + 1);
}

// Audio end
EXPORT bool CALL Encoder_IsActive() {
    // return ffm_encoder != NULL; //old encoder
    return g_encoder_active;
}

EXPORT m64p_error CALL Encoder_Start(const char* path, const char* format) {
    std::unique_lock _lock(enc_rwlock);
    if (g_encoder_active)
        return M64ERR_ALREADY_INIT;
    g_encoder_active = true;
    return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL Encoder_Stop(bool discard) {
    std::unique_lock _lock(enc_rwlock);
    if (!g_encoder_active)
        return M64ERR_NOT_INIT;
    return M64ERR_SUCCESS;
}

extern "C" void encoder_startup() {}

extern "C" void encoder_shutdown() {
    std::unique_lock _lock(enc_rwlock);
    g_encoder_active = false;
}