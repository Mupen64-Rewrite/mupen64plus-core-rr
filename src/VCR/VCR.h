//rerecording.h
//
//exposes callback functions for rest of the core

#ifndef VCR_H
#define VCR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <cstddef>

#include "api/m64p_types.h"
#include "api/m64p_plugin.h"

#ifndef BOOL
#define BOOL int
#endif

typedef enum
{
	VCR_ST_SUCCESS,
	VCR_ST_BAD_UID,
	VCR_ST_INVALID_FRAME,
	VCR_ST_WRONG_FORMAT
} VCRSTErrorCodes;

static const char* VCR_LoadStateErrors[] = {
	"no error", //0 index
	"not from this movie.",
	"frame number out of range.",
	"invalid format"
};

/// <summary>
/// Function that gets informed about various event happening in VCR, errors, messages, infos etc. It receives string to display somewhere, its up to frontend to decide where.
/// </summary>
/// <returns>True if message was handled, false if ignored (right now nothing happens when ignored, but maybe certain errors will be more flexible with this)</returns>
typedef BOOL(*MsgFunc)(m64p_msg_level lvl, const char*);

/// <summary>
/// Gives pointer to buffer that holds everything you need to save.
/// </summary>
/// <param name="dest">pointer to pointer to buffer</param>
/// <returns>Length of buffer in bytes</returns>
size_t VCR_CollectSTData(uint32_t** dest);

/// <summary>
/// Loads data from VCR part of savestate
/// </summary>
/// <param name="buf">pointer to buffer, contains frame number, vi number, movie length and input data</param>
/// <returns> error value, can be used with VCR_stateErrors[err] to get text</returns>
int VCR_LoadMovieData(uint32_t* buf, unsigned len);

/// <summary>
/// Sets any vcr error callback
/// </summary>
/// <param name="callb">Function to call when error happens</param>
void VCR_SetErrorCallback(MsgFunc callb);

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
/// Stops movie, or jumps to first frame if restart is true. When no movie is playing, restart loads up previous movie.
/// </summary>
/// <param name="restart">bool</param>
void VCR_StopMovie(BOOL restart);

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
/// Creates m64 at specified path and starts recording. Doesn't check if file exists, that's done by frontend
/// </summary>
/// <param name="path">path for new m64 (or existing, overwrites)</param>
/// <param name="author">author max 222 chars, trimmed if too much</param>
/// <param name="desc">256 max</param>
/// <param name="startType">see macros in m64.h</param>
/// /// <returns>M64ERR_FILES - something wrong with the path, otherwise M64ERR_SUCCESS</returns>
m64p_error VCR_StartRecording(char* path, char* author, char* desc, int startType);

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


