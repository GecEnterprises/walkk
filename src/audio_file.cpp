#include <cstdlib>
#include <iostream>

#define MINIMP3_IMPLEMENTATION
#include "minimp3_ex.h"

#include "audio_file.h"

int loadAudioFile(const char *filename, AudioFile &audioFile) {
	mp3dec_init(&audioFile.decoder);
	if (mp3dec_load(&audioFile.decoder, filename, &audioFile.info, NULL, NULL)) {
		std::cerr << "Error: Failed to load MP3 file" << std::endl;
		return 1;
	}
	audioFile.position = 0;
	return 0;
}

void freeAudioFile(AudioFile &audioFile) {
	if (audioFile.info.buffer) {
		free(audioFile.info.buffer);
		audioFile.info.buffer = NULL;
	}
}


