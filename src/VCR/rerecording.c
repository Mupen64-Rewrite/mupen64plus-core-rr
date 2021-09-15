//rerecording.c
//
//Implements the logic of VCR (loading and parsing movies, managing current movie state etc.)
#include "m64.h"

int GetHeaderSize()
{
	return sizeof(SMovieHeader);
}

//if (m64) play
//else no play
//
//========== Build: 0 succeeded, 1 failed, 0 skipped ==========

