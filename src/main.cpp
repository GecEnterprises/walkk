#include <iostream>
#include <vector>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <portaudio.h>

#define MINIMP3_IMPLEMENTATION
#include "minimp3_ex.h"

struct AudioData {
    mp3dec_t decoder;
    mp3dec_file_info_t info;
    size_t position;
};

// Simple sink with a thread-safe FIFO of float samples
struct AudioSink {
    std::mutex mutex;
    std::deque<float> queue;
    size_t capacity;
    std::atomic<bool> finished;

    explicit AudioSink(size_t cap)
        : capacity(cap), finished(false) {}

    size_t pop(float *out, size_t maxSamples) {
        std::lock_guard<std::mutex> lock(mutex);
        size_t available = queue.size();
        size_t toCopy = std::min(maxSamples, available);
        for (size_t i = 0; i < toCopy; i++) {
            out[i] = queue.front();
            queue.pop_front();
        }
        return toCopy;
    }

    size_t push(const float *in, size_t numSamples) {
        std::lock_guard<std::mutex> lock(mutex);
        size_t space = (capacity > queue.size()) ? (capacity - queue.size()) : 0;
        size_t toCopy = std::min(numSamples, space);
        for (size_t i = 0; i < toCopy; i++) {
            queue.push_back(in[i]);
        }
        return toCopy;
    }
};

struct CallbackData {
    AudioSink *sink;
    int channels;
};

// PortAudio callback function
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

    // We don't care about file position here; just drain from sink
    size_t samplesNeeded = framesPerBuffer * (unsigned long)cb->channels; // interleaved samples
    size_t copied = cb->sink->pop(out, samplesNeeded);

    // Fill any remainder with silence
    for (size_t i = copied; i < samplesNeeded; i++) {
        out[i] = 0.0f;
    }

    if (copied == 0 && cb->sink->finished.load()) {
        return paComplete;
    }
    return paContinue;
}

// Producer loop that feeds the sink with decoded samples as floats
static void walkkLoop(AudioData *data, AudioSink *sink, int channels) {
    const size_t totalSamples = data->info.samples;
    const int16_t *src = data->info.buffer;
    const size_t chunkSamples = (size_t)4096 * (size_t)channels; // interleaved samples per push

    std::vector<float> temp;
    temp.reserve(chunkSamples);

    size_t pos = 0;
    while (pos < totalSamples) {
        size_t remaining = totalSamples - pos;
        size_t thisChunk = std::min(chunkSamples, remaining);
        temp.clear();
        for (size_t i = 0; i < thisChunk; i++) {
            temp.push_back(src[pos + i] / 32768.0f);
        }

        size_t pushed = 0;
        while (pushed < thisChunk) {
            pushed += sink->push(temp.data() + pushed, thisChunk - pushed);
            if (pushed < thisChunk) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }

        pos += (thisChunk*0.1);
    }

    sink->finished.store(true);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <mp3_file>" << std::endl;
        return 1;
    }
    
    const char *filename = argv[1];
    AudioData audioData;
    audioData.position = 0;
    
    // Initialize decoder
    mp3dec_init(&audioData.decoder);
    
    // Decode MP3 file
    std::cout << "Loading MP3 file: " << filename << std::endl;
    if (mp3dec_load(&audioData.decoder, filename, &audioData.info, NULL, NULL)) {
        std::cerr << "Error: Failed to load MP3 file" << std::endl;
        return 1;
    }
    
    std::cout << "Sample rate: " << audioData.info.hz << " Hz" << std::endl;
    std::cout << "Channels: " << audioData.info.channels << std::endl;
    std::cout << "Samples: " << audioData.info.samples << std::endl;
    std::cout << "Duration: " << (float)audioData.info.samples / audioData.info.hz / audioData.info.channels << " seconds" << std::endl;
    
    // Initialize PortAudio
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
        free(audioData.info.buffer);
        return 1;
    }
    
    // Create sink and start producer loop
    const size_t sinkCapacity = (size_t)audioData.info.hz * (size_t)audioData.info.channels * 2; // ~2 seconds
    AudioSink sink(sinkCapacity);
    CallbackData callbackData{ &sink, audioData.info.channels };

    std::thread producer([&audioData, &sink]() {
        walkkLoop(&audioData, &sink, audioData.info.channels);
    });

    // Open audio stream
    PaStream *stream;
    err = Pa_OpenDefaultStream(&stream,
                               0,                           // no input
                               audioData.info.channels,     // output channels
                               paFloat32,                   // 32 bit float
                               audioData.info.hz,           // sample rate
                               256,                         // frames per buffer
                               paCallback,                  // callback function
                               &callbackData);              // user data
    
    if (err != paNoError) {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
        Pa_Terminate();
        free(audioData.info.buffer);
        return 1;
    }
    
    // Start playback
    std::cout << "Playing..." << std::endl;
    err = Pa_StartStream(stream);
    if (err != paNoError) {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
        Pa_CloseStream(stream);
        Pa_Terminate();
        free(audioData.info.buffer);
        return 1;
    }
    
    // Wait for playback to finish
    while (Pa_IsStreamActive(stream) == 1) {
        Pa_Sleep(100);
    }
    
    // Cleanup
    std::cout << "Playback finished." << std::endl;
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    free(audioData.info.buffer);

    if (producer.joinable()) {
        producer.join();
    }
    
    return 0;
}