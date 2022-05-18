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

static const char* VCR_LoadStateErrors[] = {
	"no error", //0 index
	"not from this movie.",
	"frame number out of range.",
	"invalid format"
};

// exported functions are defined in api/m64p_vcr.h

#ifdef __cplusplus
	}
#endif

#endif


