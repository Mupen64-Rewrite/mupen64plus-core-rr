//m64.h
//
//Describes .m64 movie header
#ifndef M64_H
#define M64_H
#include <stdint.h>

#define MOVIE_START_FROM_SNAPSHOT	1<<0
#define MOVIE_START_FROM_NOTHING	1<<1
#define MOVIE_START_FROM_EEPROM     1<<2

#define MOVIE_AUTHOR_DATA_SIZE 222
#define MOVIE_DESCRIPTION_DATA_SIZE 256

//#define MOVIE_MAX_METADATA_SIZE (MOVIE_DESCRIPTION_DATA_SIZE > MOVIE_AUTHOR_DATA_SIZE ? MOVIE_DESCRIPTION_DATA_SIZE : MOVIE_AUTHOR_DATA_SIZE)

#pragma pack(push, 1)
typedef struct
{
	uint32_t	magic;			// "M64\0x1a"
	uint32_t	version;		// 3
	uint32_t	uid;			// used to match savestates to a particular movie

	uint32_t	length_vis;		// number of "frames" in the movie
	uint32_t	rerecord_count; // amount of rerecords
	uint8_t		vis_per_second;	// "frames" per second
	uint8_t		num_controllers;
	uint16_t	reserved1;
	uint32_t	length_samples;	//number of samples for each controller (not cumulative)

	uint16_t	startFlags; //1-snapshot, 2-clean start, 4-start but with savedata
	uint16_t  reserved2;
	struct
	{
		unsigned present1 : 1; //controller 1 present
		unsigned present2 : 1; //controller 2 present
		unsigned present3 : 1; //controller 3 present
		unsigned present4 : 1; //controller 4 present

		unsigned mempak1 : 1; //controller 1 has mempak
		unsigned mempak2 : 1; //controller 2 has mempak
		unsigned mempak3 : 1; //controller 3 has mempak
		unsigned mempak4 : 1; //controller 4 has mempak

		unsigned rumble1 : 1; //controller 1 has rumblepak
		unsigned rumble2 : 1; //controller 2 has rumblepak
		unsigned rumble3 : 1; //controller 3 has rumblepak
		unsigned rumble4 : 1; //controller 4 has rumblepak
	} controllerFlags;
	uint32_t	reservedFlags[8];

	//---not supported, please use old mupen (this is really old)
	char	oldAuthorInfo[48]; //unused
	char	oldDescription[80]; //unused
	//---

	char	romName[32]; // internal rom name from header
	uint32_t	romCRC;
	uint8_t	romCountry;
	char	reservedBytes[56];
	char	videoPluginName[64];
	char	soundPluginName[64];
	char	inputPluginName[64];
	char	rspPluginName[64];
	char	authorInfo[MOVIE_AUTHOR_DATA_SIZE]; // utf8-encoded
	char	description[MOVIE_DESCRIPTION_DATA_SIZE]; // utf8-encoded


} SMovieHeader; // should be exactly 1024 bytes
#pragma pack(pop)

#endif
