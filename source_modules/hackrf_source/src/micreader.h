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
        inputParameters.sampleFormat = paFloat32;
        inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultHighInputLatency;
        inputParameters.hostApiSpecificStreamInfo = nullptr;

        err = Pa_OpenDefaultStream(&stream, 1, 0, paFloat32, SAMPLE_RATE, FRAMES_PER_BUFFER, &MicReader::paCallback, this);
        if (err != paNoError) {
            std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
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

private:
    PaStream* stream;
    PaError err;
    bool isRecording;
    CircularBuffer& circular_buffer_;

    static const int SAMPLE_RATE = 44100;
    static const int FRAMES_PER_BUFFER = 512;

    static int paCallback(const void* inputBuffer, void* outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void* userData) {
        MicReader* micReader = static_cast<MicReader*>(userData);
        const float* micBuffer = static_cast<const float*>(inputBuffer);
        size_t bufferSize = framesPerBuffer * sizeof(float);
        std::vector<uint8_t> micData(bufferSize);
        memcpy(micData.data(), micBuffer, bufferSize);
        micReader->circular_buffer_.write(micData);
        return paContinue;
    }
};

#endif // MICREADER_H
