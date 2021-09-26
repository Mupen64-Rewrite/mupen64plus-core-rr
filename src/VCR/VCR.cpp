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

#define BUFFER_GROWTH 256

//either holds current movie's path, or last movie. This could be initialised from config and have "load last m64" option?
static char moviePath[PATH_MAX] = { 0 };

static std::vector<BUTTONS> *gMovieBuffer = NULL; //holds reference to m64 data if movie is being played, otherwise NULL
static SMovieHeader* gMovieHeader = NULL;
static VCRState VCR_state = VCR_IDLE;
static bool VCR_readonly;
static unsigned curSample = 0; //doesnt account in for multiple controllers, used as a pointer for vector
static unsigned curVI = 0; //keeps track of VIs in a movie
//@TODO: add VCR_AdvanceVI();

BOOL DefaultErr(m64p_msg_level, char*);

/// <summary>
/// Calls function that displays the message somewhere. Waits for return
/// </summary>
/// <param name="lvl">Severity, see m64p_msg_level</param>
/// <param name="what">Textual representation. Translating might be supported later</param>
/// <returns>Was the message ignored?
/// </returns>
static MsgFunc VCR_Message = DefaultErr;

/// <summary>
/// default handler for errors, prints to stdout
/// </summary>
BOOL DefaultErr(m64p_msg_level lvl, char* what)
{
	DebugMessage(lvl, "%s", what);
	return TRUE;
}

void VCR_SetErrorCallback(MsgFunc callb)
{
	VCR_Message = callb;
}

/// <summary>
/// Looks at m64 header flags and does appropriate things to core before starting movie.
/// Its used in both start and restart so its hande to have it as a helper function.
/// </summary>
static void PrepareCore(char* path)
{
	if (gMovieHeader->startFlags == MOVIE_START_FROM_NOTHING)
	{
		VCR_Message(M64MSG_INFO, "Core reset");
		main_reset(true);
	}
	else if (gMovieHeader->startFlags == MOVIE_START_FROM_EEPROM)
	{
		//hard reset
		//It doesn't detect if there were any polls, so for example using this with commandline will start emu
		//and then instantly reset anyway
		//@TODO: clear/ignore save data!!
		VCR_Message(M64MSG_INFO, "Core reset");
		main_reset(true);
	}
	else if (gMovieHeader->startFlags == MOVIE_START_FROM_SNAPSHOT)
	{
		char statePath[PATH_MAX];
		strcpy(statePath, path);
		strcpy(statePath + strlen(statePath) - 4, ".st");
		VCR_Message(M64MSG_INFO, "Loading movie .st");
		main_state_load(statePath);
	}
}

void VCR_StopMovie(BOOL restart)
{
	if (!restart) //stop it
	{
		if (VCR_IsPlaying())
		{
			VCR_state = VCR_IDLE;
			delete gMovieBuffer;
			delete gMovieHeader;
			gMovieBuffer = NULL;
			gMovieHeader = NULL;
			//@TODO notify frontend more precisely, maybe with callback
			VCR_Message(M64MSG_INFO, "Playback stopped");
		}
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
		VCR_Message(M64MSG_INFO, "Playback restarted");
	}
}

//@TODO: test this
void VCR_SetKeys(BUTTONS keys, unsigned channel)
{
	if (curSample >= gMovieBuffer->size())
	{
		gMovieBuffer->resize(gMovieBuffer->size() + BUFFER_GROWTH); //impending doom approaches...
	}
	(*gMovieBuffer)[curSample++].Value = keys.Value;
	gMovieHeader->length_samples = curSample;
}

BOOL VCR_GetKeys(BUTTONS* keys, unsigned channel)
{
	//gives out inputs for channels that are present in m64 header
	//less bloated than using the flags itself
	if (gMovieHeader->cFlags.present & (1 << channel))
	{
		//check if there are frames left
		//curSample is 0 indexed, so we must >= with size
		if (curSample >= gMovieBuffer->size())
		{
			return true;
			VCR_StopMovie(true);
		}
		keys->Value = (*gMovieBuffer)[curSample++].Value; //get the value, then advance frame
	}
	if (curSample == 200)
	{
		main_state_load("loadthis.st");
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
 
//saves inputs only up to current frame!!
size_t VCR_CollectSTData(uint32_t** buf)
{
	if (!VCR_IsPlaying()) return 0; //don't try
	//curSample+1 (because 0-index), then 4*4 bytes (this could be turned to a struct but its so small that eh) 
	size_t len = (curSample + 1)*sizeof(BUTTONS) + 4 * sizeof(uint32_t);
	*buf = (uint32_t*)malloc(len); 
	if (buf == NULL || *buf==NULL) return 0;

	uint32_t* p = *buf; //cast for help
	*p++ = gMovieHeader->uid;
	*p++ = curSample;
	*p++ = curVI;
	*p++ = gMovieHeader->length_samples;
	memcpy(p, gMovieBuffer->data(), (curSample+1)*sizeof(BUTTONS));
	return len;
}

//Must be called when a movie is active
//@TODO: verify everything with debugger and add VIs
int VCR_LoadMovieData(uint32_t* buf, unsigned len)
{
	if (!VCR_IsPlaying()) return -1; //don't try
	uint32_t UID = *buf++;
	uint32_t savedCurSample = *buf++; //consider this the last sample number available, we want to write/read from next one
	uint32_t savedVI = *buf++;
	uint32_t SampleCount = *buf++;
	BUTTONS* inputs = (BUTTONS*)buf; //there are savedSampleNum+1 bytes of inputs, one frame is garbage, can be ignored (when loading old st ofc)
	if ((4 + savedCurSample +1) * 4 != len && (4 + SampleCount + 1) * 4 != len) //either savedCurSample or SampleCount should add up, otherwise its bad
		return VCR_ST_WRONG_FORMAT;
	if (UID != gMovieHeader->uid)
		return VCR_ST_BAD_UID;
	if (savedCurSample > SampleCount)
		return VCR_ST_INVALID_FRAME;
	//if readwrite, overwrite input buffer with new data, might need to resize
	if (!VCR_IsReadOnly())
	{
		//gMovieBuffer->assign(inputs, inputs + SampleCount);
		gMovieBuffer->assign(inputs, inputs + savedCurSample); //we dont care what's after this frame, there's m64 for that...
	}
	else
	{
		//means a .st was saved, then movie was shortened and the .st is attempted again.
		if (savedCurSample > gMovieHeader->length_samples)
		{
			return VCR_ST_INVALID_FRAME;
		}
	}
	curSample = savedCurSample; //continue playing from next frame, no +1 because its 0 indexed
	curVI = savedVI;
	if (!VCR_IsReadOnly()) gMovieHeader->length_samples = savedCurSample; //this is done in getkeys anyway, but for safety do it here too

	return VCR_ST_SUCCESS;
}

m64p_error VCR_StartMovie(char* path)
{
	//@TODO maybe switch to custom errors
	if (VCR_IsPlaying()) return M64ERR_INTERNAL;
	FILE *m64 = osal_file_open(path,"rb");
	if (m64 == NULL)
	{
		char msg[1024] = "LoadM64 error: ";
		strncat(msg, strerror(errno),sizeof(msg)- sizeof("LoadM64 error: "));
		VCR_Message(M64MSG_INFO, msg);
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
	VCR_Message(M64MSG_INFO, "Started playback");
	return M64ERR_SUCCESS;
}

#undef BUFFER_GROWTH

//if (m64) play
//else no play
//
//========== Build: 0 succeeded, 1 failed, 0 skipped ==========

