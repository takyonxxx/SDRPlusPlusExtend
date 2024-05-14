#ifndef RECORDER_H
#define RECORDER_H

#include "constants.h"
#include <portaudio.h>
#include <vector>
#include <queue>

class PaRecorder {
public:
    PaRecorder() : pa_stream(nullptr) {}

    void initPa() {
        const int NUM_CHANNELS = 1;
        const int SAMPLE_BLOCK_SIZE = 1024;

        err = Pa_Initialize();
        if (err != paNoError) {
            std::cerr << "PortAudio initialization failed: " << Pa_GetErrorText(err) << std::endl;
            return;
        }

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
                            SAMPLE_BLOCK_SIZE, paClipOff, audioCallback, &audioBuffer);
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

    void enqueueAudioData(const std::vector<float>& audioData) {
        std::cout << audioBufferQueue.size() << std::endl;
        std::lock_guard<std::mutex> lock(bufferMutex);
        audioBufferQueue.push(audioData);
    }

    std::vector<float> dequeueAudioData() {
        std::lock_guard<std::mutex> lock(bufferMutex);
        if (!audioBufferQueue.empty()) {
            std::vector<float> audioData = std::move(audioBufferQueue.front());
            audioBufferQueue.pop();
            return audioData;
        } else {
            return std::vector<float>(); // Return an empty vector if queue is empty
        }
    }

    std::vector<float> getAudioBuffer() const {
        return audioBuffer;
    }

private:
    static int audioCallback(const void *inputBuffer, void *outputBuffer,
                             unsigned long framesPerBuffer,
                             const PaStreamCallbackTimeInfo *timeInfo,
                             PaStreamCallbackFlags statusFlags,
                             void *userData) {
        PaRecorder* recorder = reinterpret_cast<PaRecorder*>(userData);

        const float *in = static_cast<const float*>(inputBuffer);
        recorder->audioBuffer.assign(in, in + framesPerBuffer);
        recorder->enqueueAudioData(recorder->audioBuffer);
        return paContinue;
    }

    PaStream *pa_stream;
    PaError err;
    std::vector<float> audioBuffer;
    std::queue<std::vector<float>> audioBufferQueue;
    std::mutex bufferMutex;
    PaRecorder* handler;
};

#endif // RECORDER_H
