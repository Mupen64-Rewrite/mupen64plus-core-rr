#ifndef M64P_MAIN_ENCODER_H
#define M64P_MAIN_ENCODER_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "api/m64p_types.h"

extern void encoder_startup();
extern void encoder_shutdown();

// Audio stuff
extern m64p_sample_callback* g_sample_callback;
extern m64p_rate_changed_callback* g_rate_changed_callback;

#ifdef __cplusplus
}
#endif
#endif