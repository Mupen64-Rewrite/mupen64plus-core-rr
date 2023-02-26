#ifndef M64P_API_ENCODER_H
#define M64P_API_ENCODER_H

#include <stddef.h>

extern void encoder_startup();
extern void encoder_shutdown();

extern void encoder_push_video();
extern void encoder_push_audio(void* data, size_t len);
extern void encoder_set_sample_rate(unsigned int rate);


#endif