/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus-core - m64p_vcr.h                                         *
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

#include <stdint.h>
#include "m64p_types.h"

#ifndef BOOL
  #define BOOL int
#endif

#define M64P_API_FN(RT, name, ...) \
  EXPORT RT name(__VA_ARGS__); \
  typedef RT (*ptr_##name)(__VA_ARGS__)

/**
 * Sets a hint to the encoder.
 */
M64P_API_FN(void, Encoder_Hint, m64p_encoder_setting setting, intptr_t value);
/**
 * Returns 1 if the encoder is active, 0 otherwise.
 */
M64P_API_FN(BOOL, Encoder_IsActive);

/**
 * Starts the encoder.
 */
M64P_API_FN(m64p_error, Encoder_Start, const char* path, m64p_encoder_format format);
/**
 * Stops the encoder, discarding any recorded data.
 */
M64P_API_FN(m64p_error, Encoder_Discard);
/**
 * Stops the encoder, saving all recorded frames to a video file.
 */
M64P_API_FN(m64p_error, Encoder_SaveVideo);

#undef M64P_API_FN