#pragma once

#include <deque>
#include <mutex>
#include <atomic>
#include <portaudio.h>

struct AudioSink {
	std::mutex mutex;
	std::deque<float> queue;
	size_t capacity;
	std::atomic<bool> finished;

	explicit AudioSink(size_t cap)
		: capacity(cap), finished(false) {}

	size_t pop(float *out, size_t maxSamples);
	size_t push(const float *in, size_t numSamples);
};

struct CallbackData {
	AudioSink *sink;
	int channels;
};

int openAndStartStream(PaStream **stream, CallbackData *cb, int channels, int sampleRate, unsigned long framesPerBuffer);
void stopAndCloseStream(PaStream *stream);


