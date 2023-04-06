/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus-core - m64p_vcr.h                                         *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2022 Jacky Guo                                          *
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

/* This header file defines typedefs for function pointers to rerecording
 * functions.
 */

#if !defined(M64P_VCR_H)
#define M64P_VCR_H

#include "m64p_types.h"
#include "m64p_plugin.h"
#include <stddef.h>
#include <stdint.h>

#ifndef BOOL
#define BOOL int
#endif

#ifdef __cplusplus
extern "C" {
#endif

// VCR types

typedef enum {
    M64VCRP_STATE = 0,
    M64VCRP_READONLY
} m64p_vcr_param;


/// <summary>
/// Sets any vcr error callback
/// </summary>
/// <param name="callb">Function to call when error happens</param>
typedef void (*ptr_VCR_SetErrorCallback)(BOOL (*callb)(m64p_msg_level lvl, const char*));
EXPORT void CALL VCR_SetErrorCallback(BOOL (*callb)(m64p_msg_level lvl, const char*));

typedef void (*ptr_VCR_SetStateCallback)(void (*callb)(m64p_vcr_param, int));
EXPORT void CALL VCR_SetStateCallback(void (*callb)(m64p_vcr_param, int));

//TODO: figure out needed states (I dont think stuff like starting playback is neccesary?...)
//also this stays as enum not enum class for C compatibility
typedef enum
{
	M64VCR_IDLE,
	M64VCR_ACTIVE
} m64p_vcr_state;

/// <summary>
/// Returns current frame, 0-indexed, accounts for controller;
/// </summary>
/// <returns>current frame number, or -1 if nothing is being played</returns>
typedef unsigned (*ptr_VCR_GetCurFrame)();
EXPORT unsigned CALL VCR_GetCurFrame();

/// <summary>
/// Stops movie, or jumps to first frame if restart is true. When no movie is playing, restart loads up previous movie.
/// </summary>
/// <param name="restart">bool</param>
typedef void (*ptr_VCR_StopMovie)(BOOL restart);
EXPORT void CALL VCR_StopMovie(BOOL restart);

/// <summary>
/// pastes given keys to current frame, then advances frame. If input buffer is too small it resizes itself
/// </summary>
/// <param name="keys">keys describing frame data</param>
/// <param name="channel">controller id</param>
typedef void (*ptr_VCR_SetKeys)(BUTTONS keys, unsigned channel);
EXPORT void CALL VCR_SetKeys(BUTTONS keys, unsigned channel);

/// <summary>
/// Pastes next frame to keys. If this was last frame, stops m64 playback. There always is a frame if VCR is not idle
/// </summary>
/// <param name="keys">place where to paste inputs</param>
/// <param name="channel">controller id</param>
/// <returns>true if movie ended</returns>
typedef BOOL (*ptr_VCR_GetKeys)(BUTTONS* keys, unsigned channel);
EXPORT BOOL CALL VCR_GetKeys(BUTTONS* keys, unsigned channel);

/// <summary>
/// Checks if a movie is playing
/// </summary>
/// <returns>false if idle, otherwise true</returns>
typedef BOOL (*ptr_VCR_IsPlaying)(void);
EXPORT BOOL CALL VCR_IsPlaying();

/// <summary>
/// Checks if readonly is true. Note: the value doesn't make sense if VCR is idle, it doesn't inform whether a movie is actually playing
/// </summary>
/// <see cref="VCR_IsReadOnly"/>
/// <returns>true or false</returns>
typedef BOOL (*ptr_VCR_IsReadOnly)(void);
EXPORT BOOL CALL VCR_IsReadOnly();


/// <summary>
/// sets readonly state
/// </summary>
/// <param name="state">true or false</param>
/// <returns>state</returns>
typedef BOOL (*ptr_VCR_SetReadOnly)(BOOL state);
EXPORT BOOL CALL VCR_SetReadOnly(BOOL state);

/// <summary>
/// Creates m64 at specified path and starts recording. Doesn't check if file exists, that's done by frontend
/// </summary>
/// <param name="path">path for new m64 (or existing, overwrites)</param>
/// <param name="author">author max 222 chars, trimmed if too much</param>
/// <param name="desc">256 max</param>
/// <param name="startType">see macros in m64.h</param>
/// /// <returns>M64ERR_FILES - something wrong with the path, otherwise M64ERR_SUCCESS</returns>
typedef m64p_error (*ptr_VCR_StartRecording)(const char* path, const char* author, const char* desc, int startType);
EXPORT m64p_error CALL VCR_StartRecording(const char* path, const char* author, const char* desc, int startType);

/// <summary>
/// Loads .m64 movie from path to an interal buffer and sets VCR state to VCR_PLAY.
/// </summary>
/// <param name="path">- path/to/movie.m64</param>
/// <returns>M64ERR_FILES - .m64 or .st not found, M64ERR_NO_MEMORY - out of memory, M64ERR_INTERNAL - buffer already exists, otherwise M64ERR_SUCCESS</returns>
typedef m64p_error (*ptr_VCR_StartMovie)(const char* path);
EXPORT m64p_error CALL VCR_StartMovie(const char* path);

#ifdef __cplusplus
}
#endif

#endif
