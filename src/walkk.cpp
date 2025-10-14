#include <algorithm>
#include <filesystem>
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <cmath>
#include <vector>
#include <numbers>
#include <fstream>
#include <iterator>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
#include "minimp3_ex.h"
#include "wav_writer.h"
#include "walkk.h"

namespace fs = std::filesystem;

void Walkk::addLog(const std::string &line) {
    std::lock_guard<std::mutex> lg(logMutex);
    logLines.push_back(line);
    while (logLines.size() > logMaxLines) {
        logLines.pop_front();
    }
}

int loadDirectoryMp3s(const char *directoryPath, Walkk &walkk, bool recursive) {
    try {
        // ----- Windows: keep a wide version of the directory path so we can iterate safely
        std::wstring wDirectoryPath;
        #ifdef _WIN32
        if (directoryPath) {
            int wlen = MultiByteToWideChar(CP_UTF8, 0, directoryPath, -1, nullptr, 0);
            if (wlen > 0) {
                wDirectoryPath.resize(wlen - 1);
                MultiByteToWideChar(CP_UTF8, 0, directoryPath, -1, &wDirectoryPath[0], wlen);
            }
        }
        #endif

        // ----- Remember base dir (UTF-8) for computing relative paths later
        walkk.baseDirectory = directoryPath ? std::string(directoryPath) : std::string();

        // ----- Reset stats
        {
            std::lock_guard<std::mutex> lk(walkk.loadStatsMutex);
            walkk.filesAttemptedLastLoad = 0;
            walkk.filesLoadedLast = 0;
        }

        auto handleEntry = [&](const fs::directory_entry &entry) {
            std::error_code ec;
            if (!entry.is_regular_file(ec)) return;

            const auto path = entry.path();
            if (!path.has_extension()) return;

            // Case-insensitive .mp3 check
            std::string ext = path.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });
            if (ext != ".mp3") return;

            {
                std::lock_guard<std::mutex> lk(walkk.loadStatsMutex);
                walkk.filesAttemptedLastLoad++;
            }

            StreamedFile file;

            // ----- Store UTF-8 absolute path into file.path
            #ifdef _WIN32
            {
                std::wstring wpath = path.wstring();
                int len = WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    file.path.resize(len - 1);
                    WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, &file.path[0], len, nullptr, nullptr);
                } else {
                    file.path = path.string(); // fallback
                }
            }
            #else
            file.path = path.string(); // already UTF-8 on Unix-y platforms
            #endif

            // ----- Compute a nice relative path for display
            try {
                fs::path relPath = fs::relative(path, fs::path(walkk.baseDirectory));
                #ifdef _WIN32
                std::wstring wrel = relPath.wstring();
                int len = WideCharToMultiByte(CP_UTF8, 0, wrel.c_str(), -1, nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    file.relPath.resize(len - 1);
                    WideCharToMultiByte(CP_UTF8, 0, wrel.c_str(), -1, &file.relPath[0], len, nullptr, nullptr);
                } else {
                    file.relPath = relPath.string();
                }
                #else
                file.relPath = relPath.string();
                #endif
            } catch (...) {
                // Fallback: just the filename
                #ifdef _WIN32
                std::wstring wname = path.filename().wstring();
                int len = WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    file.relPath.resize(len - 1);
                    WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, &file.relPath[0], len, nullptr, nullptr);
                } else {
                    file.relPath = path.filename().string();
                }
                #else
                file.relPath = path.filename().string();
                #endif
            }

            // ----- Open the file with minimp3_ex (streaming from disk, no slurp)
            bool ok = false;
            #ifdef _WIN32
            {
                std::wstring wpath = path.wstring();
                // MP3D_SEEK_TO_SAMPLE builds seek index up front; remove if you want even faster scan
                ok = (mp3dec_ex_open_w(&file.decoder, wpath.c_str(), MP3D_SEEK_TO_SAMPLE) == 0);
            }
            #else
            ok = (mp3dec_ex_open(&file.decoder, file.path.c_str(), MP3D_SEEK_TO_SAMPLE) == 0);
            #endif

            if (ok) {
                file.sampleRate  = file.decoder.info.hz;
                file.channels    = file.decoder.info.channels;
                file.totalFrames = file.decoder.samples / std::max(1, file.channels);
                file.isOpen = true;

                // Close immediately; reopen on-demand later when playing/reading grains
                mp3dec_ex_close(&file.decoder);
                file.isOpen = false;

                walkk.files.push_back(std::move(file));
                {
                    std::lock_guard<std::mutex> lk(walkk.loadStatsMutex);
                    walkk.filesLoadedLast++;
                }
                std::string msg = std::string("Loaded: ") + walkk.files.back().relPath +
                                  " (" + std::to_string(walkk.files.back().totalFrames) + " frames)";
                std::cout << msg << std::endl;
                walkk.addLog(msg);
            } else {
                std::string msg = std::string("Failed to load: ") + file.relPath;
                std::cerr << msg << std::endl;
                walkk.addLog(msg);
            }
        };

        // ----- Build the directory path to iterate
        fs::path dirPath;
        #ifdef _WIN32
        dirPath = fs::path(wDirectoryPath);
        #else
        dirPath = fs::path(directoryPath ? directoryPath : "");
        #endif

        // Iterate (skip permission-denied entries so one bad folder doesn't abort the whole scan)
        if (recursive) {
            for (const auto &entry :
                 fs::recursive_directory_iterator(dirPath,
                     fs::directory_options::skip_permission_denied)) {
                // Guard per-entry to avoid aborting the whole walk on one bad file
                try { handleEntry(entry); } catch (...) {}
            }
        } else {
            for (const auto &entry :
                 fs::directory_iterator(dirPath,
                     fs::directory_options::skip_permission_denied)) {
                try { handleEntry(entry); } catch (...) {}
            }
        }

        // ----- Summarize
        size_t tried = 0, loaded = 0;
        {
            std::lock_guard<std::mutex> lk(walkk.loadStatsMutex);
            tried  = walkk.filesAttemptedLastLoad;
            loaded = walkk.filesLoadedLast;
        }
        std::string sum = "Scan complete. Tried=" + std::to_string(tried) +
                          " loaded=" + std::to_string(loaded);
        std::cout << sum << std::endl;
        walkk.addLog(sum);

        return walkk.files.empty() ? 1 : 0;
    } catch (const std::exception &e) {
        std::string msg = std::string("Error reading directory: ") + e.what();
        std::cerr << msg << std::endl;
        walkk.addLog(msg);
        return 1;
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
    
    // Snapshot settings under lock to avoid tearing while generating a grain
    Walkk::GranularSettings settingsSnapshot;
    {
        std::lock_guard<std::mutex> lock(walkk.settingsMutex);
        settingsSnapshot = walkk.settings;
    }

    if (settingsSnapshot.minGrainMs > settingsSnapshot.maxGrainMs) {
        std::swap(settingsSnapshot.minGrainMs, settingsSnapshot.maxGrainMs);
    }

    std::uniform_int_distribution<size_t> durationDist(settingsSnapshot.minGrainMs, settingsSnapshot.maxGrainMs);
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
    std::bernoulli_distribution loopDist(std::clamp(settingsSnapshot.loopProbability, 0.0f, 1.0f));
    grain.loopEnabled = loopDist(walkk.rng);

    if (grain.loopEnabled) {
        // Loop window is randomized per grain
        size_t minWin = std::min(settingsSnapshot.minLoopWindowMs, settingsSnapshot.maxLoopWindowMs);
        size_t maxWin = std::max(settingsSnapshot.minLoopWindowMs, settingsSnapshot.maxLoopWindowMs);
        std::uniform_int_distribution<size_t> loopWinMsDist(minWin, maxWin);

        size_t loopWindowMs = loopWinMsDist(walkk.rng);
        // Window is in *source* frames (before resample), we’ll convert right after we know the ratio
        // For now store ms and convert later in readGrain (needs file.sampleRate)
        // But to keep params self-contained, convert here using the file’s native rate:
        grain.loopWindowFrames = std::max<size_t>(1, (loopWindowMs * (size_t)file.sampleRate) / 1000);

        // Drag is signed and also in *source* frames
        int maxDrag = std::max(0, settingsSnapshot.maxLoopDragMs);
        std::uniform_int_distribution<int> dragMsDist(-maxDrag, maxDrag);
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

    // Snapshot overlap once; could also be sampled per-grain if desired
    size_t overlapMsSnapshot;
    {
        std::lock_guard<std::mutex> lock(walkk->settingsMutex);
        overlapMsSnapshot = walkk->settings.grainOverlapMs;
    }
    const size_t overlapFrames = (overlapMsSnapshot * (size_t)Walkk::kSampleRate) / 1000;
    (void)overlapFrames;

    std::vector<float> grainBuffer;
    std::vector<float> noiseBuffer;

    while (!walkk->allFinished.load()) {
        GrainParams grain = generateRandomGrain(*walkk);

        {
            std::string fname = (grain.fileIndex < walkk->files.size()) ? walkk->files[grain.fileIndex].relPath : std::string("?");
            std::string gmsg = "next>>>" + std::to_string(grain.fileIndex) +
                               " (" + fname + ") start=" + std::to_string(grain.startFrame) +
                               " dur=" + std::to_string(grain.durationFrames) + "f" +
                               " amp=" + std::to_string(grain.amplitude) +
                               (grain.loopEnabled ? " loop=on" : " loop=off");
            std::cout << gmsg << std::endl;
            walkk->addLog(gmsg);
        }

        // Update last grain debug info for GUI
        {
            std::lock_guard<std::mutex> g(walkk->lastGrainMutex);
            walkk->lastGrain.fileIndex = grain.fileIndex;
            if (grain.fileIndex < walkk->files.size()) {
                const auto &sf = walkk->files[grain.fileIndex];
                if (!sf.relPath.empty()) {
                    walkk->lastGrain.relPath = sf.relPath;
                } else {
                    // Fallback to basename if relPath missing
                    try {
                        walkk->lastGrain.relPath = fs::path(sf.path).filename().string();
                    } catch (...) {
                        walkk->lastGrain.relPath = sf.path;
                    }
                }
            } else {
                walkk->lastGrain.relPath.clear();
            }
            walkk->lastGrain.startFrame = grain.startFrame;
            walkk->lastGrain.durationFrames = grain.durationFrames;
            walkk->lastGrain.amplitude = grain.amplitude;
            walkk->lastGrain.loopEnabled = grain.loopEnabled;
            walkk->lastGrain.loopWindowFrames = grain.loopWindowFrames;
            walkk->lastGrain.loopDragFrames = grain.loopDragFrames;
        }

        if (!readGrain(walkk->files[grain.fileIndex], grain, grainBuffer, Walkk::kSampleRate)) {
            std::cerr << "Failed to read grain" << std::endl;
            continue;
        }

        const size_t chunkFrames = 512;
        size_t framesPushed = 0;

        // On first push, estimate when these samples will reach audio out based on queue depth
        bool startEstimated = false;
        while (framesPushed < grain.durationFrames) {
            size_t framesToPush = std::min(chunkFrames, grain.durationFrames - framesPushed);
            size_t sampleOffset = framesPushed * (size_t)Walkk::kChannels;
            size_t samplesToPush = framesToPush * (size_t)Walkk::kChannels;

            size_t pushed = 0;
            while (pushed < samplesToPush) {
                if (!startEstimated) {
                    size_t queued = walkk->sink.getQueuedSamples();
                    double secondsQueue = (double)queued / (double)(Walkk::kSampleRate * Walkk::kChannels);
                    auto eta = std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                        std::chrono::duration<double>(secondsQueue));
                    {
                        std::lock_guard<std::mutex> g(walkk->lastGrainMutex);
                        walkk->lastGrain.expectedStartTime = eta;
                        walkk->lastGrain.hasExpectedStart = true;
                        walkk->lastGrain.hasStarted = false;
                        // Estimated end time = start + duration
                        double secondsDur = (double)grain.durationFrames / (double)Walkk::kSampleRate;
                        walkk->lastGrain.expectedEndTime = eta + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                            std::chrono::duration<double>(secondsDur));
                    }
                    startEstimated = true;
                }
                pushed += walkk->sink.push(grainBuffer.data() + sampleOffset + pushed,
                                           samplesToPush - pushed);
                if (pushed < samplesToPush) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }

            framesPushed += framesToPush;
        }

        // Optional white noise between grains
        size_t noiseMsSnapshot = 0;
        float noiseAmpSnapshot = 0.25f;
        {
            std::lock_guard<std::mutex> lock(walkk->settingsMutex);
            noiseMsSnapshot = std::min<size_t>(5000, walkk->settings.whiteNoiseMs);
            noiseAmpSnapshot = std::max(0.0f, std::min(1.0f, walkk->settings.whiteNoiseAmplitude));
        }

        if (noiseMsSnapshot > 0) {
            size_t noiseFrames = (noiseMsSnapshot * (size_t)Walkk::kSampleRate) / 1000;
            noiseBuffer.resize(noiseFrames * (size_t)Walkk::kChannels);

            std::uniform_real_distribution<float> noiseDist(-noiseAmpSnapshot, noiseAmpSnapshot);
            for (size_t f = 0; f < noiseFrames; ++f) {
                float nL = noiseDist(walkk->rng);
                float nR = noiseDist(walkk->rng);
                noiseBuffer[f * 2 + 0] = nL;
                noiseBuffer[f * 2 + 1] = nR;
            }

            size_t pushed = 0;
            size_t samplesToPush = noiseFrames * (size_t)Walkk::kChannels;
            while (pushed < samplesToPush) {
                pushed += walkk->sink.push(noiseBuffer.data() + pushed,
                                           samplesToPush - pushed);
                if (pushed < samplesToPush) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
            walkk->sink.finished.store(true);
        }
    }
}

double Walkk::getRecordingDurationSeconds() {
    if (!isRecording.load()) {
        return 0.0;
    }

    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::duration<double>>(now - recordingStartTime);
    return duration.count();
}

bool Walkk::startRecording(const std::string& outputPath) {
    std::lock_guard<std::mutex> lock(recordingMutex);

    if (isRecording.load()) {
        return false; // Already recording
    }

    // Open WAV file for writing
    recordingFile = fopen(outputPath.c_str(), "wb");
    if (!recordingFile) {
        addLog("Failed to open recording file: " + outputPath);
        return false;
    }

    // Initialize WAV header
    WavHeader header;
    initWavHeader(&header, kSampleRate, kChannels, 16);

    // Write header with placeholder sizes
    if (!writeWavHeader(recordingFile, &header)) {
        fclose(recordingFile);
        recordingFile = nullptr;
        addLog("Failed to write WAV header");
        return false;
    }

    recordingOutputPath = outputPath;
    recordingDataSize = 0;
    recordingStartTime = std::chrono::steady_clock::now();
    isRecording.store(true);

    addLog("Started recording to: " + outputPath);
    return true;
}

void Walkk::stopRecording() {
    std::lock_guard<std::mutex> lock(recordingMutex);

    if (!isRecording.load()) {
        return;
    }

    // Update WAV header with actual data size
    if (recordingFile) {
        if (fseek(recordingFile, 0, SEEK_SET) == 0) {
            updateWavHeader(recordingFile, recordingDataSize);
        }
        fclose(recordingFile);
        recordingFile = nullptr;
    }

    isRecording.store(false);
    addLog("Stopped recording. Total size: " + std::to_string(recordingDataSize) + " bytes");
}

void Walkk::writeRecordingData(const float* data, size_t frames) {
    if (!isRecording.load() || !recordingFile) {
        return;
    }

    std::lock_guard<std::mutex> lock(recordingMutex);

    if (writeWavAudioData(recordingFile, data, frames, kChannels)) {
        recordingDataSize += frames * kChannels * sizeof(int16_t);
    }
}
