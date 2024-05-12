#ifndef RECORDER_H
#define RECORDER_H
#include "constants.h"
#include <portaudio.h>

std::vector<float> readMicrophoneBuffer() {

    const int NUM_CHANNELS = 1;
    const int SAMPLE_BLOCK_SIZE = 1024;

    PaError err;
    PaStream *pa_stream = nullptr;
    std::vector<float> audioBuffer(BUF_LEN * BYTES_PER_SAMPLE);

    err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio initialization failed: " << Pa_GetErrorText(err) << std::endl;
        return audioBuffer;
    }

    err = Pa_OpenDefaultStream(&pa_stream, NUM_CHANNELS, 0, paFloat32, AUDIO_SAMPLE_RATE, SAMPLE_BLOCK_SIZE, nullptr, nullptr);
    if (err != paNoError) {
        std::cerr << "PortAudio stream opening error: " << Pa_GetErrorText(err) << std::endl;
        Pa_Terminate();
        return audioBuffer;
    }

    err = Pa_StartStream(pa_stream);
    if (err != paNoError) {
        std::cerr << "PortAudio stream starting error: " << Pa_GetErrorText(err) << std::endl;
        Pa_CloseStream(pa_stream); // Close the stream
        Pa_Terminate();
        return audioBuffer;
    }

    err = Pa_ReadStream(pa_stream, audioBuffer.data(), BUF_LEN * BYTES_PER_SAMPLE);
    if (err != paNoError) {
        std::cerr << "PortAudio read stream error: " << Pa_GetErrorText(err) << std::endl;
        return audioBuffer;
    }

    err = Pa_StopStream(pa_stream);
    if (err != paNoError) {
        std::cerr << "PortAudio stream stopping error: " << Pa_GetErrorText(err) << std::endl;
        return audioBuffer;
    }

    err = Pa_CloseStream(pa_stream);
    if (err != paNoError) {
        std::cerr << "PortAudio stream closing error: " << Pa_GetErrorText(err) << std::endl;
        return audioBuffer;
    }

    pa_stream = nullptr;
    err = Pa_Terminate();
    if (err != paNoError) {
        std::cerr << "PortAudio termination error: " << Pa_GetErrorText(err) << std::endl;
        return audioBuffer;
    }

    return audioBuffer;
}
#endif // RECORDER_H
