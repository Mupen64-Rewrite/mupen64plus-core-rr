/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus-core - m64p_encoder.h                                     *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2024 Jacky Guo                                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* This header file defines typedefs for function pointers to encoder
 * functions.
 */
#ifndef M64P_ENCODER_H
#define M64P_ENCODER_H
#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>
#include <stdbool.h>


#ifndef ENC_SUPPORT
#define ENC_SUPPORT
#endif
#include "m64p_types.h"

#ifndef M64P_ENCODER_PROTOTYPES
#define M64P_API_FN(RT, name, ...) \
  typedef RT (*ptr_##name)(__VA_ARGS__)
#else
#define M64P_API_FN(RT, name, ...) \
  typedef RT (*ptr_##name)(__VA_ARGS__); \
  EXPORT RT CALL name(__VA_ARGS__)
#endif

typedef enum {
  M64ENC_FORMAT = 0,
  M64ENC_VIDEO,
  M64ENC_AUDIO,
} m64p_encoder_hint_type;

/**
 * Returns 1 if the encoder is active, 0 otherwise.
 */
M64P_API_FN(bool, Encoder_IsActive);
/**
 * Starts the encoder. To apply settings, set and save settings in the following sections:
 * - EncFFmpeg-Video
 * - EncFFmpeg-Audio
 * - EncFFmpeg-Format
 * These sections correspond to the video codec, audio codec, and muxer respectively.
 * If this function raises an error, no encode will be started.
 */
M64P_API_FN(m64p_error, Encoder_Start, const char* path, const char* format);
/**
 * Stops the encoder. If discard is true, discards data.
 */
M64P_API_FN(m64p_error, Encoder_Stop, bool discard);

// Audio callbacks

/*
 * Registers a callback that will be called when new sample is available.
 * The called function receives pointer to the sample and size in bytes.
 */
M64P_API_FN(m64p_error, Encoder_SetSampleCallback, SampleCallback* callback);
M64P_API_FN(
    m64p_error, Encoder_SetRateChangedCallback, RateChangedCallback* callback
);
M64P_API_FN(unsigned int, Encoder_GetSampleRate, void);

#undef M64P_API_FN

#ifdef __cplusplus
}
#endif
#endif