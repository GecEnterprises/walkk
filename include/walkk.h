#pragma once

#include <vector>
#include <atomic>
#include <random>
#include <string>

#include "minimp3_ex.h"
#include "pa_sink.h"

// Stream-based file info (no full buffer loaded)
struct StreamedFile {
	std::string path;
	mp3dec_ex_t decoder;
	size_t totalFrames;
	int sampleRate;
	int channels;
	bool isOpen;
	
	StreamedFile() : totalFrames(0), sampleRate(0), channels(0), isOpen(false) {}
	
	~StreamedFile() {
		if (isOpen) {
			mp3dec_ex_close(&decoder);
		}
	}
	
	// Prevent copying (decoder can't be copied)
	StreamedFile(const StreamedFile&) = delete;
	StreamedFile& operator=(const StreamedFile&) = delete;
	
	// Allow moving
	StreamedFile(StreamedFile&& other) noexcept 
		: path(std::move(other.path))
		, decoder(other.decoder)
		, totalFrames(other.totalFrames)
		, sampleRate(other.sampleRate)
		, channels(other.channels)
		, isOpen(other.isOpen) {
		other.isOpen = false;
	}
	
	StreamedFile& operator=(StreamedFile&& other) noexcept {
		if (this != &other) {
			if (isOpen) {
				mp3dec_ex_close(&decoder);
			}
			path = std::move(other.path);
			decoder = other.decoder;
			totalFrames = other.totalFrames;
			sampleRate = other.sampleRate;
			channels = other.channels;
			isOpen = other.isOpen;
			other.isOpen = false;
		}
		return *this;
	}
};

struct GrainParams {
    size_t fileIndex;
    size_t startFrame;      // Frame position in file to start grain
    size_t durationFrames;  // Length of grain in frames
    float  amplitude;       // Grain volume

    // New: funny loop params (per-grain)
    bool   loopEnabled = false;
    size_t loopWindowFrames = 0;  // window size in src frames (pre-resample domain)
    int    loopDragFrames = 0;    // signed shift applied to window start on each wrap
};

struct Walkk {
    // Fixed sink: 48kHz stereo
    static const int kSampleRate = 48000;
    static const int kChannels   = 2;

    AudioSink sink;
    std::vector<StreamedFile> files;
    std::atomic<bool> allFinished;

    // Grain parameters (configurable)
    size_t minGrainMs = 50;      // Min grain duration in milliseconds
    size_t maxGrainMs = 1200;     // Max grain duration in milliseconds
    size_t grainOverlapMs = 20;  // Overlap between grains in milliseconds
    size_t maxConcurrentGrains = 4;

    // New: global controls for loop behavior
    float  loopProbability = 0.0f;  // 0..1, exposed externally

    // Loop window & drag ranges (in milliseconds / frames domain described below)
    size_t minLoopWindowMs = 20;     // typical tiny windows feel “scrubby”
    size_t maxLoopWindowMs = 620;
    int    maxLoopDragMs   = 25;     // ± range for drag

    // Random number generator
    std::mt19937 rng;

    explicit Walkk(size_t sinkCapacity)
        : sink(sinkCapacity), allFinished(false), rng(std::random_device{}()) {}
};


// Load all .mp3 files from directoryPath into walkk.files (streaming mode)
// If recursive is true, traverse subdirectories recursively.
int loadDirectoryMp3s(const char *directoryPath, Walkk &walkk, bool recursive = false);

// Producer loop: granulizer that plays random segments from random files
void granulizerLoop(Walkk *walkk);