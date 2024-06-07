#ifndef PORTAUDIOSOURCE_H
#define PORTAUDIOSOURCE_H

#include <iostream>
#include <portaudio.h>
#include "circular_buffer.h"

class PortAudioSource {
public:
    PortAudioSource(CircularBuffer& circular_buffer) : circular_buffer_(circular_buffer), stream(nullptr), isRecording(false)
    {
        err = Pa_Initialize();
        if (err != paNoError) {
            std::cerr << "PortAudioSource initialization failed: " << Pa_GetErrorText(err) << std::endl;
            return;
        }

        PaStreamParameters inputParameters;
        inputParameters.device = Pa_GetDefaultInputDevice();
        if (inputParameters.device == paNoDevice) {
            std::cerr << "Error: No default input device found!" << std::endl;
            Pa_Terminate();
            return;
        }

        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(inputParameters.device);
        std::cout << "Using input device: " << deviceInfo->name << std::endl;

        inputParameters.channelCount = 1; // Mono input
        inputParameters.sampleFormat = paInt8;
        inputParameters.suggestedLatency = deviceInfo->defaultHighInputLatency;
        inputParameters.hostApiSpecificStreamInfo = nullptr;

//        PaStreamParameters outputParameters;
//        outputParameters.device = Pa_GetDefaultOutputDevice();
//        if (outputParameters.device == paNoDevice) {
//            std::cerr << "Error: No default output device found!" << std::endl;
//            Pa_Terminate();
//            return;
//        }
//        const PaDeviceInfo* outputDeviceInfo = Pa_GetDeviceInfo(outputParameters.device);
//        std::cout << "Using output device: " << outputDeviceInfo->name << std::endl;

//        outputParameters.channelCount = 1; // Mono output
//        outputParameters.sampleFormat = paInt32;
//        outputParameters.suggestedLatency = outputDeviceInfo->defaultLowOutputLatency;
//        outputParameters.hostApiSpecificStreamInfo = nullptr;

        err = Pa_OpenStream(&stream, &inputParameters, nullptr, SAMPLE_RATE, paFramesPerBufferUnspecified, paClipOff, &PortAudioSource::paCallback, this);
        if (err != paNoError) {
            std::cerr << "Failed to open audio stream: " << Pa_GetErrorText(err) << std::endl;
            Pa_Terminate();
            return;
        }
    }

    ~PortAudioSource()
    {
        if (stream) {
            Pa_CloseStream(stream);
            Pa_Terminate();
        }
        stream = nullptr;
    }

    bool start()
    {
        err = Pa_StartStream(stream);
        if (err != paNoError) {
            std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
            Pa_Terminate();
            return false;
        }
        isRecording = true;
        std::cerr << "PortAudioSource started." << std::endl;
        return true;
    }

    bool stop()
    {
        if (stream) {
            Pa_StopStream(stream);
        }
        std::cout << "PortAudioSource stopped." << std::endl;
        isRecording = false;        
        return true;
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
        PortAudioSource* _source = static_cast<PortAudioSource*>(userData);
        const int8_t* in = static_cast<const int8_t*>(inputBuffer);
        int8_t* out = static_cast<int8_t*>(outputBuffer);

        if (in != nullptr) {
            std::vector<uint8_t> write_data(in, in + framesPerBuffer);
            _source->circular_buffer_.write(write_data);
        }
        return paContinue;
    }
};

#endif // PORTAUDIOSOURCE_H
