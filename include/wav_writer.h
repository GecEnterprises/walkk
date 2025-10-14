#pragma once

#include <cstdint>
#include <cstdio>

// WAV file header structure
#pragma pack(push, 1)
struct WavHeader {
    char riff[4];           // "RIFF"
    uint32_t fileSize;      // File size - 8
    char wave[4];           // "WAVE"
    char fmt[4];            // "fmt "
    uint32_t chunkSize;     // 16 for PCM
    uint16_t audioFormat;   // 1 for PCM
    uint16_t numChannels;   // 1 for mono, 2 for stereo
    uint32_t sampleRate;    // Sample rate
    uint32_t byteRate;      // SampleRate * NumChannels * BitsPerSample/8
    uint16_t blockAlign;    // NumChannels * BitsPerSample/8
    uint16_t bitsPerSample; // 16 or 32
    char data[4];           // "data"
    uint32_t dataSize;      // Size of data section
};
#pragma pack(pop)

// Initialize WAV header for 16-bit stereo audio at 48kHz
void initWavHeader(WavHeader* header, uint32_t sampleRate = 48000, uint16_t channels = 2, uint16_t bitsPerSample = 16);

// Write WAV header to file (with placeholder sizes)
bool writeWavHeader(FILE* file, const WavHeader* header);

// Update WAV header with actual file sizes (for when recording is complete)
bool updateWavHeader(FILE* file, uint32_t dataSize);

// Write 16-bit PCM audio data to WAV file
bool writeWavAudioData(FILE* file, const float* audioData, size_t frameCount, uint16_t channels = 2);

// Convert float audio data to 16-bit integers for WAV writing
void convertFloatToInt16(const float* input, int16_t* output, size_t sampleCount);
