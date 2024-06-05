#ifndef MICREADER_H
#define MICREADER_H

#include <iostream>
#include <portaudio.h>
#include "circular_buffer.h"

class MicReader {
public:
    MicReader(CircularBuffer& circular_buffer) : circular_buffer_(circular_buffer), stream(nullptr), isRecording(false) {}

    ~MicReader() {
        if (stream) {
            Pa_CloseStream(stream);
            Pa_Terminate();
        }
    }

    bool init() {
        err = Pa_Initialize();
        if (err != paNoError) {
            std::cerr << "PortAudio initialization failed: " << Pa_GetErrorText(err) << std::endl;
            return false;
        }

        PaStreamParameters inputParameters;
        inputParameters.device = Pa_GetDefaultInputDevice();
        if (inputParameters.device == paNoDevice) {
            std::cerr << "Error: No default input device found!" << std::endl;
            Pa_Terminate();
            return false;
        }
        inputParameters.channelCount = 1; // Mono input
        inputParameters.sampleFormat = paInt8;
        inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultHighInputLatency;
        inputParameters.hostApiSpecificStreamInfo = nullptr;

        if (Pa_OpenStream(&stream, &inputParameters, nullptr, SAMPLE_RATE, paFramesPerBufferUnspecified, paClipOff, &MicReader::paCallback, this) != paNoError) {
            std::cerr << "Failed to open audio stream" << std::endl;
            Pa_Terminate();
            return false;
        }
        return true;
    }

    bool startRecord() {

        err = Pa_StartStream(stream);
        if (err != paNoError) {
            std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
            Pa_Terminate();
            return false;
        }

        isRecording = true;
        std::cerr << "PortAudio record started." << std::endl;
        return true;
    }

    void stopRecord() {
        if (stream) {
            Pa_StopStream(stream);
        }
        std::cout << "PortAudio record stopped." << std::endl;
        isRecording = false;
    }

    void finish(){

        if (stream) {
            Pa_CloseStream(stream);
            Pa_Terminate();
        }
        stream = nullptr;
    }

    std::vector<uint8_t> getInstantMicBuffer(size_t size) {
        size_t numFrames = size / sizeof(float);
        std::vector<float> floatBuffer(numFrames);
        std::vector<uint8_t> byteBuffer(size);

        err = Pa_ReadStream(stream, floatBuffer.data(), numFrames);
        if (err != paNoError) {
            std::cerr << "PortAudio read error: " << Pa_GetErrorText(err) << std::endl;
            byteBuffer.clear();
            return byteBuffer;
        }
        memcpy(byteBuffer.data(), floatBuffer.data(), size);

        return byteBuffer;
    }

private:
    PaStream* stream;
    PaError err;
    bool isRecording;
    CircularBuffer& circular_buffer_;

    static const int SAMPLE_RATE = 44100;
    static const int FRAMES_PER_BUFFER = 256;

    static int paCallback(const void* inputBuffer, void* outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void* userData) {
        MicReader* micReader = static_cast<MicReader*>(userData);        
        const int8_t* in = static_cast<const int8_t*>(inputBuffer);
        for (unsigned long i = 0; i < framesPerBuffer; ++i) {
            {
                std::lock_guard<std::mutex> lock(micReader->circular_buffer_.mutex_);
                micReader->circular_buffer_.buffer_[micReader->circular_buffer_.head_] = in[i];
                micReader->circular_buffer_.head_ = (micReader->circular_buffer_.head_ + 1) % micReader->circular_buffer_.buffer_.size();
            }
            micReader->circular_buffer_.data_available_.notify_one();
        }
        return paContinue;
    }
};

#endif // MICREADER_H
