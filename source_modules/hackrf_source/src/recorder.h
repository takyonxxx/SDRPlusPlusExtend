#ifndef RECORDER_H
#define RECORDER_H

#include "constants.h"
#include <portaudio.h>
#include <vector>
#include <queue>
#include <mutex>
#include <iostream>

static std::queue<std::vector<float>> s_audioBufferQueue;
static std::mutex s_audioBufferMutex;
static std::condition_variable s_audioBufferCondition;

class PaRecorder {
public:
    PaRecorder() : pa_stream(nullptr) {}
    const int NUM_CHANNELS = 1;
    const int SAMPLE_BLOCK_SIZE = 1024;

    void initPa() {
        err = Pa_Initialize();
        if (err != paNoError) {
            std::cerr << "PortAudio initialization failed: " << Pa_GetErrorText(err) << std::endl;
            return;
        }

        // Initialize mutex for the buffer queue
        s_audioBufferMutex.lock();
        s_audioBufferMutex.unlock();

        // Find the default input device index
        PaDeviceIndex inputDevice = Pa_GetDefaultInputDevice();
        if (inputDevice == paNoDevice) {
            std::cerr << "No default input device found!" << std::endl;
            Pa_Terminate();
            return;
        }

        // Set parameters for input stream
        PaStreamParameters inputParams;
        inputParams.device = inputDevice;
        inputParams.channelCount = NUM_CHANNELS;
        inputParams.sampleFormat = paFloat32;
        inputParams.suggestedLatency = Pa_GetDeviceInfo(inputDevice)->defaultLowInputLatency;
        inputParams.hostApiSpecificStreamInfo = nullptr;

        // Open stream with input parameters
        err = Pa_OpenStream(&pa_stream, &inputParams, nullptr, AUDIO_SAMPLE_RATE,
                            SAMPLE_BLOCK_SIZE, paClipOff, audioCallback, this);
        if (err != paNoError) {
            std::cerr << "PortAudio stream opening error: " << Pa_GetErrorText(err) << std::endl;
            Pa_Terminate();
            return;
        }

        std::cout << "PortAudio initialized." << std::endl;
    }

    void startPaCallback() {
        err = Pa_StartStream(pa_stream);
        if (err != paNoError) {
            std::cerr << "PortAudio stream starting error: " << Pa_GetErrorText(err) << std::endl;
            Pa_CloseStream(pa_stream); // Close the stream
            Pa_Terminate();
        }
        std::cout << "PortAudio stream started." << std::endl;
    }

    void stopPaCallback() {
        err = Pa_StopStream(pa_stream);
        if (err != paNoError) {
            std::cerr << "PortAudio stream stopping error: " << Pa_GetErrorText(err) << std::endl;
        }
        std::cout << "PortAudio stream stopped." << std::endl;
    }

    void finishPa() {
        err = Pa_CloseStream(pa_stream);
        if (err != paNoError) {
            std::cerr << "PortAudio stream closing error: " << Pa_GetErrorText(err) << std::endl;
            return;
        }

        pa_stream = nullptr;
        err = Pa_Terminate();
        if (err != paNoError) {
            std::cerr << "PortAudio termination error: " << Pa_GetErrorText(err) << std::endl;
            return;
        }
        std::cout << "PortAudio finished." << std::endl;
    }

private:
    static int audioCallback(const void *inputBuffer, void *outputBuffer,
                             unsigned long framesPerBuffer,
                             const PaStreamCallbackTimeInfo *timeInfo,
                             PaStreamCallbackFlags statusFlags,
                             void *userData) {
        PaRecorder* recorder = reinterpret_cast<PaRecorder*>(userData);

        const float *in = static_cast<const float*>(inputBuffer);

        std::vector<float> audioBuffer(in, in + framesPerBuffer);

        // Lock the buffer queue for thread-safe access
        std::lock_guard<std::mutex> lock(s_audioBufferMutex);
        s_audioBufferQueue.push(audioBuffer);

        // Notify waiting threads that new data is available
        s_audioBufferCondition.notify_one();

//        // Once s_audioBufferQueue is not empty, proceed
//        std::vector<float> audioData = std::move(s_audioBufferQueue.front());
//        s_audioBufferQueue.pop();
//        std::cout << audioData.size() << std::endl;

        return paContinue;
    }

    PaStream *pa_stream;
    PaError err;
};
#endif // RECORDER_H
