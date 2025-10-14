#include "wav_writer.h"
#include <cstring>
#include <algorithm>

void initWavHeader(WavHeader* header, uint32_t sampleRate, uint16_t channels, uint16_t bitsPerSample) {
    // Initialize RIFF header
    std::memcpy(header->riff, "RIFF", 4);
    header->fileSize = 0; // Will be updated later
    std::memcpy(header->wave, "WAVE", 4);

    // Initialize format chunk
    std::memcpy(header->fmt, "fmt ", 4);
    header->chunkSize = 16;
    header->audioFormat = 1; // PCM
    header->numChannels = channels;
    header->sampleRate = sampleRate;
    header->bitsPerSample = bitsPerSample;

    // Calculate byte rate and block align
    header->byteRate = sampleRate * channels * (bitsPerSample / 8);
    header->blockAlign = channels * (bitsPerSample / 8);

    // Initialize data chunk (with placeholder size)
    std::memcpy(header->data, "data", 4);
    header->dataSize = 0; // Will be updated later
}

bool writeWavHeader(FILE* file, const WavHeader* header) {
    return fwrite(header, sizeof(WavHeader), 1, file) == 1;
}

bool updateWavHeader(FILE* file, uint32_t dataSize) {
    WavHeader header;
    if (fread(&header, sizeof(WavHeader), 1, file) != 1) {
        return false;
    }

    // Update sizes
    header.dataSize = dataSize;
    header.fileSize = dataSize + sizeof(WavHeader) - 8;

    // Seek back to beginning and write updated header
    fseek(file, 0, SEEK_SET);
    return fwrite(&header, sizeof(WavHeader), 1, file) == 1;
}

bool writeWavAudioData(FILE* file, const float* audioData, size_t frameCount, uint16_t channels) {
    const size_t sampleCount = frameCount * channels;

    // Convert float samples to 16-bit integers
    int16_t* intData = new int16_t[sampleCount];
    convertFloatToInt16(audioData, intData, sampleCount);

    // Write the data
    const size_t bytesToWrite = sampleCount * sizeof(int16_t);
    const bool success = fwrite(intData, 1, bytesToWrite, file) == bytesToWrite;

    delete[] intData;
    return success;
}

void convertFloatToInt16(const float* input, int16_t* output, size_t sampleCount) {
    for (size_t i = 0; i < sampleCount; ++i) {
        // Clamp to [-1.0, 1.0] and convert to 16-bit
        float sample = std::max(-1.0f, std::min(1.0f, input[i]));
        output[i] = static_cast<int16_t>(sample * 32767.0f);
    }
}
