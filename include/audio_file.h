#pragma once

#include "minimp3_ex.h"

struct AudioFile {
	mp3dec_t decoder;
	mp3dec_file_info_t info;
	size_t position;
};

// Loads an MP3 file into AudioFile.info.buffer and initializes fields.
// Returns 0 on success, non-zero on error.
int loadAudioFile(const char *filename, AudioFile &audioFile);

// Frees any allocated buffers within AudioFile
void freeAudioFile(AudioFile &audioFile);


