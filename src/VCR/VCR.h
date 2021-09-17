//rerecording.h
//
//exposes callback functions for rest of the core

#ifndef VCR_H
#define VCR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "api/m64p_types.h"
#include "api/m64p_plugin.h"

//TODO: figure out needed states (I dont think stuff like starting playback is neccesary?...)
//also this stays as enum not enum class for C compatibility
typedef enum
{
	VCR_IDLE,
	VCR_ACTIVE
} VCRState;

/// <summary>
/// Returns current frame, 0-indexed, accounts for controller;
/// </summary>
/// <returns>current frame number, or -1 if nothing is being played</returns>
unsigned VCR_GetCurFrame();
/// <summary>
/// Stops movie and frees the internal buffer, can be called at emu close to clean up
/// </summary>
void VCR_StopMovie();

/// <summary>
/// pastes given keys to current frame, then advances frame. If input buffer is too small it resizes itself
/// </summary>
/// <param name="keys">keys describing frame data</param>
/// <param name="channel">controller id</param>
void VCR_SetKeys(BUTTONS keys, unsigned channel);

/// <summary>
/// Pastes next frame to keys. If this was last frame, stops m64 playback. There always is a frame if VCR is not idle
/// </summary>
/// <param name="keys">place where to paste inputs</param>
/// <param name="channel">controller id</param>
/// <returns>true if movie ended</returns>
BOOL VCR_GetKeys(BUTTONS* keys, unsigned channel);

/// <summary>
/// Checks if a movie is playing
/// </summary>
/// <returns>false if idle, otherwise true</returns>
BOOL VCR_IsPlaying();

/// <summary>
/// Checks if readonly is true. Note: the value doesn't make sense if VCR is idle, it doesn't inform whether a movie is actually playing
/// </summary>
/// <see cref="VCR_IsPlaying"/>
/// <returns>true or false</returns>
BOOL VCR_IsReadOnly();

/// <summary>
/// sets readonly state
/// </summary>
/// <param name="state">true or false</param>
/// <returns>state</returns>
BOOL VCR_SetReadOnly(BOOL state);

/// <summary>
/// Loads .m64 movie from path to an interal buffer and sets VCR state to VCR_PLAY.
/// </summary>
/// <param name="path">- path/to/movie.m64</param>
/// <returns>M64ERR_FILES - .m64 or .st not found, M64ERR_NO_MEMORY - out of memory, M64ERR_INTERNAL - buffer already exists, otherwise M64ERR_SUCCESS</returns>
m64p_error VCR_StartMovie(char* path);

#ifdef __cplusplus
	}
#endif

#endif


