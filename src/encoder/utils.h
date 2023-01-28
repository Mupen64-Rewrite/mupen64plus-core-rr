#ifndef M64P_ENCODER_UTILS_H
#define M64P_ENCODER_UTILS_H

#include "api/m64p_types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Given a path, attempts to infer a video encoding for that format.
m64p_encoder_format infer_encode_format(const char* path);

#ifdef __cplusplus
}
#endif
#endif