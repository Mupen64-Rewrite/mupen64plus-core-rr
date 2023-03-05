#ifndef M64P_MAIN_ENCODER_H
#define M64P_MAIN_ENCODER_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "api/m64p_types.h"

extern void encoder_startup();
extern void encoder_shutdown();

extern m64p_error encoder_push_video();
extern m64p_error encoder_push_audio(void* data, size_t len);
extern m64p_error encoder_set_sample_rate(unsigned int rate);

#ifdef __cplusplus
}
#endif
#endif