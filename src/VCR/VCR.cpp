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
}

static std::vector<BUTTONS> *gMovieBuffer = NULL; //holds reference to m64 data if movie is being played, otherwise NULL
static VCRState VCR_state = VCRState::VCR_IDLE;
static bool VCR_readonly;

BOOL VCR_IsPlaying()
{
	return VCR_state != VCRState::VCR_IDLE;
}

BOOL VCR_GetReadOnly()
{
	return VCR_readonly;
}

BOOL VCR_SetReadOnly(BOOL state)
{
	return VCR_readonly = state;
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
	SMovieHeader movieHeader; //@TODO move to heap if header will be needed in other places
	fread(&movieHeader,sizeof(SMovieHeader),1,m64);

	//this should never happen under normal conditions, means code logic error
	if (gMovieBuffer != NULL)
		return M64ERR_INTERNAL;

	//create a vector with enough space
	gMovieBuffer = new std::vector<BUTTONS>(movieHeader.length_samples * movieHeader.num_controllers);
	if (m64 == NULL)
		return M64ERR_NO_MEMORY;

	VCR_state = VCR_GetReadOnly() ? VCRState::VCR_READONLY : VCRState::VCR_READWRITE;
	return M64ERR_SUCCESS;
}

//if (m64) play
//else no play
//
//========== Build: 0 succeeded, 1 failed, 0 skipped ==========

