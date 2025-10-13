#include <algorithm>
#include <filesystem>
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <cmath>
#include <vector>

#include "minimp3_ex.h"
#include "walkk.h"

namespace fs = std::filesystem;

int loadDirectoryMp3s(const char *directoryPath, Walkk &walkk, bool recursive) {
    try {
        auto handleEntry = [&](const fs::directory_entry &entry) {
            if (!entry.is_regular_file()) return;
            auto path = entry.path();
            if (!path.has_extension()) return;
            auto ext = path.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext != ".mp3") return;

            StreamedFile file;
            file.path = path.string();

            // Open briefly to read metadata, then close to avoid pinning memory per file
            if (mp3dec_ex_open(&file.decoder, file.path.c_str(), MP3D_SEEK_TO_SAMPLE) == 0) {
                file.sampleRate = file.decoder.info.hz;
                file.channels = file.decoder.info.channels;
                file.totalFrames = file.decoder.samples / std::max(1, file.channels);
                file.isOpen = true;

                // Immediately close; we will reopen on-demand when reading grains
                mp3dec_ex_close(&file.decoder);
                file.isOpen = false;

                walkk.files.push_back(std::move(file));
                std::cout << "Loaded: " << path.string()
                          << " (" << walkk.files.back().totalFrames << " frames)" << std::endl;
            } else {
                std::cerr << "Failed to load: " << path.string() << std::endl;
            }
        };

        if (recursive) {
            for (const auto &entry : fs::recursive_directory_iterator(directoryPath)) {
                handleEntry(entry);
            }
        } else {
            for (const auto &entry : fs::directory_iterator(directoryPath)) {
                handleEntry(entry);
            }
        }
        return walkk.files.empty() ? 1 : 0;
    } catch (const std::exception &e) {
        std::cerr << "Error reading directory: " << e.what() << std::endl;
        return 1;
    }
}

static void applyGrainEnvelope(float *samples, size_t numFrames, size_t channels) {
    for (size_t i = 0; i < numFrames; i++) {
        float t = (numFrames > 1) ? (float)i / (float)(numFrames - 1) : 0.0f;
        float envelope = 0.5f * (1.0f - std::cos(2.0f * M_PI * t));
        for (size_t ch = 0; ch < channels; ch++) {
            samples[i * channels + ch] *= envelope;
        }
    }
}

static bool readGrain(StreamedFile &file, GrainParams &params, std::vector<float> &output, int targetRate) {
    // Lazily open decoder if needed to keep memory footprint low across many files
    bool openedHere = false;
    if (!file.isOpen) {
        if (mp3dec_ex_open(&file.decoder, file.path.c_str(), MP3D_SEEK_TO_SAMPLE) != 0) {
            return false;
        }
        file.sampleRate = file.decoder.info.hz;
        file.channels = file.decoder.info.channels;
        file.totalFrames = file.decoder.samples / std::max(1, file.channels);
        file.isOpen = true;
        openedHere = true;
    }

    // Resample ratio: src -> dst
    double rateRatio = (double)file.sampleRate / (double)targetRate;

    // How many source frames correspond to the requested destination length?
    size_t nominalSrcFrames = (size_t)std::ceil(params.durationFrames * rateRatio) + 2;

    // If not looping, keep previous behavior but read a safe span once.
    const bool useLoop = params.loopEnabled && params.loopWindowFrames >= 2;

    // Estimate loop hits to budget headroom for drag
    size_t windowLen = useLoop ? params.loopWindowFrames : nominalSrcFrames;
    windowLen = std::max<size_t>(2, windowLen);

    // Loop count estimate: how many times we’ll likely wrap
    size_t estWraps = useLoop ? (nominalSrcFrames / windowLen) + 2 : 0;

    // Worst-case net displacement if drag is always in the same direction
    int64_t drag = (int64_t)params.loopDragFrames;
    int64_t worstDisp = (int64_t)estWraps * (int64_t)std::llabs(drag);

    // Build a read window with head/tail margins to survive scrubbing
    int64_t baseStart = (int64_t)params.startFrame;
    int64_t headroom  = useLoop ? worstDisp + 8 : 0; // allow backward drags
    int64_t tailroom  = useLoop ? (int64_t)nominalSrcFrames + worstDisp + 8 : (int64_t)nominalSrcFrames + 8;

    // Clamp the read range to the file
    int64_t readStart = std::max<int64_t>(0, baseStart - headroom);
    int64_t readEnd   = std::min<int64_t>((int64_t)file.totalFrames, baseStart + tailroom);
    if (readEnd <= readStart) return false;

    size_t readFrames = (size_t)(readEnd - readStart);

    // Seek & read the contiguous source slice
    uint64_t seekSample = (uint64_t)readStart * (uint64_t)file.channels;
    if (mp3dec_ex_seek(&file.decoder, seekSample) != 0) {
        if (openedHere) {
            mp3dec_ex_close(&file.decoder);
            file.isOpen = false;
        }
        return false;
    }

    std::vector<mp3d_sample_t> srcBuffer(readFrames * (size_t)file.channels);
    size_t samplesRead = mp3dec_ex_read(&file.decoder, srcBuffer.data(), readFrames * (size_t)file.channels);
    size_t framesRead  = samplesRead / (size_t)file.channels;
    if (framesRead < 2) {
        if (openedHere) {
            mp3dec_ex_close(&file.decoder);
            file.isOpen = false;
        }
        return false;
    }

    // Helpers to clamp mapped indices into [0, framesRead-1] of our local buffer
    auto clampLocal = [&](int64_t f) -> size_t {
        if (f < 0) return 0;
        if (f >= (int64_t)framesRead - 1) return framesRead - 2; // leave room for i1
        return (size_t)f;
    };

    auto readSample = [&](size_t frame, int ch) -> float {
        int srcCh = (file.channels == 1) ? 0 : ch;
        return (float)srcBuffer[frame * (size_t)file.channels + (size_t)srcCh] / 32768.0f;
    };

    // Precompute where "our local zero" is relative to file start
    const int64_t localZeroFileFrame = readStart;

    output.resize(params.durationFrames * (size_t)Walkk::kChannels);

    // State for loop window
    int64_t winStartFileFrame = baseStart;  // in file frames
    size_t  winLen            = windowLen;  // in source frames

    for (size_t dstFrame = 0; dstFrame < params.durationFrames; ++dstFrame) {
        // Position in *source* frames if we were reading linearly
        double srcPosLin = (double)dstFrame * rateRatio; // [0..nominalSrcFrames)

        int64_t srcFileFrame;
        if (useLoop) {
            // Reduce srcPosLin into repeated windows, shifting by drag on each wrap
            // Compute wraps and remainder without a loop (fast math)
            double wrapsD   = std::floor(srcPosLin / (double)winLen);
            size_t wraps    = (wrapsD < 0) ? 0 : (size_t)wrapsD;
            double inWinPos = srcPosLin - (double)wraps * (double)winLen;
            if (inWinPos < 0.0) inWinPos = 0.0; // safety

            // Window start after `wraps` shifts
            int64_t shiftedStart = winStartFileFrame + (int64_t)wraps * drag;

            // Clamp window start to file
            if (shiftedStart < 0) shiftedStart = 0;
            if (shiftedStart + (int64_t)winLen >= (int64_t)file.totalFrames)
                shiftedStart = std::max<int64_t>(0, (int64_t)file.totalFrames - (int64_t)winLen - 1);

            srcFileFrame = shiftedStart + (int64_t)inWinPos;
        } else {
            srcFileFrame = baseStart + (int64_t)srcPosLin;
        }

        // Map to local buffer coordinates
        int64_t localFrame = srcFileFrame - localZeroFileFrame;
        size_t i0 = clampLocal(localFrame);
        size_t i1 = i0 + 1;
        double frac = (double)localFrame - (double)i0;

        float l0 = readSample(i0, 0);
        float l1 = readSample(i1, 0);
        float r0 = readSample(i0, 1);
        float r1 = readSample(i1, 1);

        float L = (float)((1.0 - frac) * l0 + frac * l1) * params.amplitude;
        float R = (float)((1.0 - frac) * r0 + frac * r1) * params.amplitude;

        output[dstFrame * 2 + 0] = L;
        output[dstFrame * 2 + 1] = R;
    }

    // applyGrainEnvelope(output.data(), params.durationFrames, 2);

    // Close decoder if we opened it for this grain to avoid keeping many files mapped
    if (openedHere) {
        mp3dec_ex_close(&file.decoder);
        file.isOpen = false;
    }
    return true;
}


static GrainParams generateRandomGrain(Walkk &walkk) {
    GrainParams grain{};

    std::uniform_int_distribution<size_t> fileDist(0, walkk.files.size() - 1);
    grain.fileIndex = fileDist(walkk.rng);

    StreamedFile &file = walkk.files[grain.fileIndex];

    std::uniform_int_distribution<size_t> durationDist(walkk.minGrainMs, walkk.maxGrainMs);
    size_t durationMs = durationDist(walkk.rng);
    grain.durationFrames = (durationMs * (size_t)Walkk::kSampleRate) / 1000;

    if (file.totalFrames > grain.durationFrames) {
        std::uniform_int_distribution<size_t> posDist(0, file.totalFrames - grain.durationFrames);
        grain.startFrame = posDist(walkk.rng);
    } else {
        grain.startFrame = 0;
        grain.durationFrames = file.totalFrames;
    }

    std::uniform_real_distribution<float> ampDist(0.3f, 0.7f);
    grain.amplitude = ampDist(walkk.rng);

    // --- New: decide whether this grain uses looping, and set window/drag ---
    std::bernoulli_distribution loopDist(std::clamp(walkk.loopProbability, 0.0f, 1.0f));
    grain.loopEnabled = loopDist(walkk.rng);

    if (grain.loopEnabled) {
        // Loop window is randomized per grain
        std::uniform_int_distribution<size_t> loopWinMsDist(
            walkk.minLoopWindowMs, walkk.maxLoopWindowMs);

        size_t loopWindowMs = loopWinMsDist(walkk.rng);
        // Window is in *source* frames (before resample), we’ll convert right after we know the ratio
        // For now store ms and convert later in readGrain (needs file.sampleRate)
        // But to keep params self-contained, convert here using the file’s native rate:
        grain.loopWindowFrames = std::max<size_t>(1, (loopWindowMs * (size_t)file.sampleRate) / 1000);

        // Drag is signed and also in *source* frames
        std::uniform_int_distribution<int> dragMsDist(- (int)walkk.maxLoopDragMs, (int)walkk.maxLoopDragMs);
        int dragMs = dragMsDist(walkk.rng);
        grain.loopDragFrames = (int)((int64_t)dragMs * (int64_t)file.sampleRate / 1000);

        // Safety: cap window to something sensible w.r.t. file size
        if (grain.loopWindowFrames > file.totalFrames / 4) {
            grain.loopWindowFrames = std::max<size_t>(1, file.totalFrames / 4);
        }
    } else {
        grain.loopWindowFrames = 0;
        grain.loopDragFrames   = 0;
    }

    return grain;
}


void granulizerLoop(Walkk *walkk) {
    if (walkk->files.empty()) {
        walkk->sink.finished.store(true);
        walkk->allFinished.store(true);
        return;
    }

    const size_t overlapFrames = (walkk->grainOverlapMs * (size_t)Walkk::kSampleRate) / 1000;
    (void)overlapFrames;

    std::vector<float> grainBuffer;

    while (!walkk->allFinished.load()) {
        GrainParams grain = generateRandomGrain(*walkk);

        std::cout << "Grain: file=" << grain.fileIndex
                  << " start=" << grain.startFrame
                  << " dur=" << grain.durationFrames << "f" << std::endl;

        if (!readGrain(walkk->files[grain.fileIndex], grain, grainBuffer, Walkk::kSampleRate)) {
            std::cerr << "Failed to read grain" << std::endl;
            continue;
        }

        const size_t chunkFrames = 512;
        size_t framesPushed = 0;

        while (framesPushed < grain.durationFrames) {
            size_t framesToPush = std::min(chunkFrames, grain.durationFrames - framesPushed);
            size_t sampleOffset = framesPushed * (size_t)Walkk::kChannels;
            size_t samplesToPush = framesToPush * (size_t)Walkk::kChannels;

            size_t pushed = 0;
            while (pushed < samplesToPush) {
                pushed += walkk->sink.push(grainBuffer.data() + sampleOffset + pushed,
                                           samplesToPush - pushed);
                if (pushed < samplesToPush) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }

            framesPushed += framesToPush;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    walkk->sink.finished.store(true);
}
