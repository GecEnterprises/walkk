#pragma once

#include <deque>
#include <mutex>
#include <atomic>
#include <portaudio.h>

struct Walkk; // Forward declaration

struct AudioSink {
	std::mutex mutex;
	std::deque<float> queue;
	size_t capacity;
	std::atomic<bool> finished;

	explicit AudioSink(size_t cap)
		: capacity(cap), finished(false) {}

	size_t pop(float *out, size_t maxSamples);
	size_t push(const float *in, size_t numSamples);

	// Thread-safe query of queued samples
	size_t getQueuedSamples();
};

struct CallbackData {
	AudioSink *sink;
	int channels;
	Walkk *walkk; // Added for recording functionality
};

int openAndStartStream(PaStream **stream, CallbackData *cb, int channels, int sampleRate, unsigned long framesPerBuffer);
void stopAndCloseStream(PaStream *stream);

float getCurrentAmplitude();
