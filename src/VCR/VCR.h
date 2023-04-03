//rerecording.h
//
//exposes callback functions for rest of the core

#ifndef VCR_H
#define VCR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

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

extern const char* VCR_LoadStateErrors[];

// exported functions are defined in api/m64p_vcr.h


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

#ifdef __cplusplus
	}
#endif

#endif
