// rerecording.c
//
// Implements the logic of VCR (loading and parsing movies, managing current movie state etc.)

#include "m64.h"
#include "VCR.h"

#include <cstdio>
#include <cerrno>
#include <vector>

extern "C" {
#include "api/m64p_types.h"
#include "api/callbacks.h"
#include "api/m64p_plugin.h" //for BUTTON struct
#include "osal/files.h"
#include "main/main.h"
}

//either holds current movie's path, or last movie. This could be initialised from config and have "load last m64" option?
static char moviePath[PATH_MAX] = { 0 };

static std::vector<BUTTONS> *gMovieBuffer = NULL; //holds reference to m64 data if movie is being played, otherwise NULL
static SMovieHeader* gMovieHeader = NULL;
static VCRState VCR_state = VCR_IDLE;
static bool VCR_readonly;
static unsigned curSample = 0; //doesnt account in for multiple controllers, used as a pointer for vector



#define BUFFER_GROWTH 256

/// <summary>
/// Looks at m64 header flags and does appropriate things to core before starting movie.
/// Its used in both start and restart so its hande to have it as a helper function.
/// </summary>
static void PrepareCore(char* path)
{
	if (gMovieHeader->startFlags == MOVIE_START_FROM_NOTHING)
		main_reset(true);
	else if (gMovieHeader->startFlags == MOVIE_START_FROM_EEPROM)
		//hard reset
		//It doesn't detect if there were any polls, so for example using this with commandline will start emu
		//and then instantly reset anyway
		//@TODO: clear/ignore save data!!
		main_reset(true);
	else if (gMovieHeader->startFlags == MOVIE_START_FROM_SNAPSHOT)
	{
		char statePath[PATH_MAX];
		strcpy(statePath, path);
		strcpy(statePath + strlen(statePath) - 4, ".st");
		main_state_load(statePath); //@TODO check file errors
	}
}

void VCR_StopMovie(BOOL restart)
{
	if (!restart) //stop it
	{
		VCR_state = VCR_IDLE;
		delete gMovieBuffer;
		delete gMovieHeader;
		gMovieBuffer = NULL;
		gMovieHeader = NULL;
	}
	//if path exists
	else if(moviePath[0]!='\0')
	{
		if (VCR_IsPlaying()) //no need to restart everything, just roll back
		{
			PrepareCore(moviePath);
			curSample = 0;
			VCR_SetReadOnly(true);
		}
		else //nothing is playing, load up saved path
		{
			VCR_StartMovie(moviePath);
		}
	}
}


void VCR_SetKeys(BUTTONS keys, unsigned channel)
{
	(*gMovieBuffer)[curSample++].Value = keys.Value;
	if (curSample >= gMovieBuffer->size())
	{
		gMovieBuffer->resize(gMovieBuffer->size() + BUFFER_GROWTH); //impending doom approaches...
	}
}

BOOL VCR_GetKeys(BUTTONS* keys, unsigned channel)
{
	//gives out inputs for channels that are present in m64 header
	//less bloated than using the flags itself
	if (gMovieHeader->cFlags.present & (1 << channel))
	{
		keys->Value = (*gMovieBuffer)[curSample++].Value; //get the value, then advance frame
		//the frame we just provided could be last, check for bounds.
		//frames are 0-indexed, size is 1-indexed, therefore >=
		if (curSample >= gMovieBuffer->size())
		{
			//@TODO somehow notify frontend about that
			VCR_StopMovie(true);
			return true; //ended
		}
	}
	return false;
}

BOOL VCR_IsPlaying()
{
	return VCR_state != VCR_IDLE;
}

BOOL VCR_IsReadOnly()
{
	return VCR_readonly;
}

BOOL VCR_SetReadOnly(BOOL state)
{
	return VCR_readonly = state;
}

unsigned VCR_GetCurFrame()
{
	if (!VCR_IsPlaying()) return -1;
	return curSample / gMovieHeader->num_controllers;
}

m64p_error VCR_StartMovie(char* path)
{
	//@TODO close previous m64 if loading while vcr state is not idle (ask warning in frontend or here, depends)
	FILE *m64 = osal_file_open(path,"rb");
	if (m64 == NULL)
	{
		DebugMessage(M64MSG_INFO, "LoadM64 error: %s", strerror(errno));
		return M64ERR_FILES;
	}
	gMovieHeader = new SMovieHeader; //@TODO move to heap if header will be needed in other places
	fread(gMovieHeader,sizeof(SMovieHeader),1,m64);

	//this should never happen under normal conditions, means code logic error
	if (gMovieBuffer != NULL)
	{
		fclose(m64);
		return M64ERR_INTERNAL;
	}

	//create a vector with enough space
	gMovieBuffer = new std::vector<BUTTONS>(gMovieHeader->length_samples * gMovieHeader->num_controllers);
	if (m64 == NULL)
	{
		fclose(m64);
		return M64ERR_NO_MEMORY;
	}
	fread(gMovieBuffer->data(), sizeof(BUTTONS), gMovieBuffer->size(),m64);
	fclose(m64);

	PrepareCore(path);

	//remember it
	strcpy(moviePath,path);
	VCR_state = VCR_ACTIVE;
	curSample = 0;
	VCR_SetReadOnly(true);
	return M64ERR_SUCCESS;
}

#undef BUFFER_GROWTH

//if (m64) play
//else no play
//
//========== Build: 0 succeeded, 1 failed, 0 skipped ==========

