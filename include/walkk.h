#pragma once

#include <vector>
#include <atomic>
#include <random>
#include <string>
#include <mutex>
#include <deque>

#include "minimp3_ex.h"
#include "pa_sink.h"

// Stream-based file info (no full buffer loaded)
struct StreamedFile {
	std::string path;
	std::string relPath; // path relative to last loaded base directory
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
		, relPath(std::move(other.relPath))
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
			relPath = std::move(other.relPath);
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

    // Last loaded directory and counts
    std::string baseDirectory;
    size_t filesAttemptedLastLoad = 0; // how many files we tried to load (matching extension)
    size_t filesLoadedLast = 0;        // how many successfully opened
    std::mutex loadStatsMutex;         // guard the counters during background loading

    struct GranularSettings {
        size_t minGrainMs = 50;
        size_t maxGrainMs = 1200;
        size_t grainOverlapMs = 20;
        size_t maxConcurrentGrains = 4;
        float  loopProbability = 0.0f; // 0..1
        size_t minLoopWindowMs = 20;
        size_t maxLoopWindowMs = 620;
        int    maxLoopDragMs   = 25;   // ± ms
        size_t whiteNoiseMs    = 0;    // silence replaced by white noise between grains
        float  whiteNoiseAmplitude = 0.25f; // 0..1 amplitude for noise
    } settings;

    std::mutex settingsMutex;

    // Debug/status of the most recently generated grain (for GUI display)
    struct GrainDebugInfo {
        size_t fileIndex = 0;
        std::string relPath;
        size_t startFrame = 0;
        size_t durationFrames = 0;
        float amplitude = 0.0f;
        bool loopEnabled = false;
        size_t loopWindowFrames = 0;
        int loopDragFrames = 0;
        // Estimated audio-out start time for this grain (steady_clock)
        std::chrono::steady_clock::time_point expectedStartTime;
        bool hasExpectedStart = false;
        bool hasStarted = false;
        std::chrono::steady_clock::time_point expectedEndTime;
    } lastGrain;
    std::mutex lastGrainMutex;

    // Explicit current-playing grain snapshot (promoted when ETA is reached)
    GrainDebugInfo currentGrain;

    // Console-like log buffer for GUI history
    std::deque<std::string> logLines;
    std::mutex logMutex;
    size_t logMaxLines = 2000;

    void addLog(const std::string &line);

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