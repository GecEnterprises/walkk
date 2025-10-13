#include <iostream>
#include <vector>
#include <cmath>
#include <thread>
#include <chrono>
#include "audio_file.h"
#include "pa_sink.h"
#include "walkk.h"

// Audio data handled via AudioFile (see audio_file.h)

// PortAudio plumbing moved to pa_sink.{h,cpp}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <directory_with_mp3s>" << std::endl;
        return 1;
    }
    
    const char *filename = argv[1];
    // Create Walkk with fixed sink and load directory of mp3s
    const int kSinkRate = 48000;
    const int kSinkChannels = 2;
    const size_t sinkCapacity = (size_t)kSinkRate * (size_t)kSinkChannels * 2; // ~2 seconds
    Walkk walkk(sinkCapacity);
    if (loadDirectoryMp3s(filename, walkk) != 0 || walkk.files.empty()) {
        std::cerr << "No MP3 files loaded from directory: " << filename << std::endl;
        return 1;
    }
    CallbackData callbackData{ &walkk.sink, kSinkChannels };

    std::thread producer([&walkk]() {
        granulizerLoop(&walkk);
    });

    // Open audio stream
    PaStream *stream;
    int err = openAndStartStream(&stream, &callbackData, kSinkChannels, kSinkRate, 256);
    if (err != paNoError) {
        std::cerr << "PortAudio error: " << err << std::endl;
        return 1;
    }
    
    // Stream already started by openAndStartStream
    std::cout << "Playing..." << std::endl;
    
    // Wait for playback to finish
    while (Pa_IsStreamActive(stream) == 1) {
        Pa_Sleep(100);
    }
    
    // Cleanup
    std::cout << "Playback finished." << std::endl;
    stopAndCloseStream(stream);

    if (producer.joinable()) {
        producer.join();
    }
    
    return 0;
}