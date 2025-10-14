#include <algorithm>

#include <portaudio.h>

#include "pa_sink.h"
#include "walkk.h"

size_t AudioSink::pop(float *out, size_t maxSamples) {
	std::lock_guard<std::mutex> lock(mutex);
	size_t available = queue.size();
	size_t toCopy = std::min(maxSamples, available);
	for (size_t i = 0; i < toCopy; i++) {
		out[i] = queue.front();
		queue.pop_front();
	}
	return toCopy;
}

size_t AudioSink::push(const float *in, size_t numSamples) {
	std::lock_guard<std::mutex> lock(mutex);
	size_t space = (capacity > queue.size()) ? (capacity - queue.size()) : 0;
	size_t toCopy = std::min(numSamples, space);
	for (size_t i = 0; i < toCopy; i++) {
		queue.push_back(in[i]);
	}
	return toCopy;
}

size_t AudioSink::getQueuedSamples() {
	std::lock_guard<std::mutex> lock(mutex);
	return queue.size();
}

static int paCallback(const void *inputBuffer, void *outputBuffer,
						unsigned long framesPerBuffer,
						const PaStreamCallbackTimeInfo* timeInfo,
						PaStreamCallbackFlags statusFlags,
						void *userData)
{
	(void)inputBuffer;
	(void)timeInfo;
	(void)statusFlags;

	CallbackData *cb = (CallbackData*)userData;
	float *out = (float*)outputBuffer;

	size_t samplesNeeded = framesPerBuffer * (unsigned long)cb->channels;
	size_t copied = cb->sink->pop(out, samplesNeeded);
	for (size_t i = copied; i < samplesNeeded; i++) {
		out[i] = 0.0f;
	}

	if (copied == 0 && cb->sink->finished.load()) {
		return paComplete;
	}

	// Check if we need to record this data
	// We need to access the Walkk instance somehow. For now, we'll assume
	// the CallbackData has a pointer to Walkk, but we need to modify the struct
	if (cb->walkk && cb->walkk->isRecording.load()) {
		size_t framesCopied = copied / cb->channels;
		cb->walkk->writeRecordingData(out, framesCopied);
	}

	return paContinue;
}

int openAndStartStream(PaStream **stream, CallbackData *cb, int channels, int sampleRate, unsigned long framesPerBuffer) {
	PaError err = Pa_Initialize();
	if (err != paNoError) {
		return err;
	}

	err = Pa_OpenDefaultStream(stream,
					0,
					channels,
					paFloat32,
					sampleRate,
					framesPerBuffer,
					paCallback,
					cb);
	if (err != paNoError) {
		Pa_Terminate();
		return err;
	}

	err = Pa_StartStream(*stream);
	if (err != paNoError) {
		Pa_CloseStream(*stream);
		Pa_Terminate();
		return err;
	}
	return paNoError;
}

void stopAndCloseStream(PaStream *stream) {
	if (!stream) return;
	Pa_StopStream(stream);
	Pa_CloseStream(stream);
	Pa_Terminate();
}


