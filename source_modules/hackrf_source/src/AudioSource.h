#ifndef AUDIOSOURCE_H
#define AUDIOSOURCE_H
#pragma once

#include <iostream>
#include <stdexcept>
#include <RtAudio.h>
#include <string.h>
#include "types.h"
#include "stream_tx.h"
#include <atomic>
#include <mutex>

class RtAudioSource {
public:
    RtAudioSource(dsp::stream_tx<dsp::complex_tx>& stream_tx,
                  unsigned int sampleRate = 44100,
                  unsigned int framesPerBuffer = 4096)
        :
          stream_tx(stream_tx),
          sampleRate(sampleRate),
          framesPerBuffer(framesPerBuffer),
          audio(),
          selectedDevice(-1),
          inputChannels(0),
          isRunning(false)
    {
        if (audio.getDeviceCount() < 1) {
            throw std::runtime_error("No audio devices found!");
        }

        selectDevice();
        setupStream();
    }

    ~RtAudioSource() {
        stop();
        if (audio.isStreamOpen()) {
            audio.closeStream();
        }
    }

    bool start() {
        std::lock_guard<std::mutex> lock(mutex);
        if (!isRunning) {
            try {
                audio.startStream();
                isRunning = true;
                std::cout << "Audio Input Stream Started "  << std::endl;
                return true;
            }
#if RTAUDIO_VERSION_MAJOR >= 6
            catch (const RtAudioErrorType& e) {
                std::cerr << "Error starting stream: " << audio.getErrorText() << std::endl;
#else
            catch (const RtAudioError& e) {
                std::cerr << "Error starting stream: " << e.what() << std::endl;
#endif
                return false;
            }
        }
        return true;
    }

    bool stop() {
        std::lock_guard<std::mutex> lock(mutex);
        if (isRunning) {
            try {
                audio.stopStream();
                isRunning = false;
                std::cout << "Audio Input Stream Stopped "  << std::endl;
                return true;
            }
#if RTAUDIO_VERSION_MAJOR >= 6
            catch (const RtAudioErrorType& e) {
                std::cerr << "Error stopping stream: " << audio.getErrorText() << std::endl;
#else
            catch (const RtAudioError& e) {
                std::cerr << "Error stopping stream: " << e.what() << std::endl;
#endif
                return false;
            }
        }
        return true;
    }

    bool getIsRunning() const { return isRunning; }


private:
    dsp::stream_tx<dsp::complex_tx>& stream_tx;
    unsigned int sampleRate;
    unsigned int framesPerBuffer;
    RtAudio audio;
    int selectedDevice;
    int inputChannels;
    std::atomic<bool> isRunning;
    std::mutex mutex;

    void selectDevice() {
        RtAudio::DeviceInfo info;
        int defaultInputDevice = audio.getDefaultInputDevice();

               // First, try to use the default input device
        if (defaultInputDevice != 0) {
            try {
                info = audio.getDeviceInfo(defaultInputDevice);
                if (info.inputChannels > 0) {
                    selectedDevice = defaultInputDevice;
                    inputChannels = info.inputChannels;
                    std::cout << "Using default input device: " << info.name << " (ID: " << selectedDevice << ")" << std::endl;
                    return;
                }
            }
#if RTAUDIO_VERSION_MAJOR >= 6
            catch (const RtAudioErrorType& e) {
                std::cerr << "Error getting default input device info: " << audio.getErrorText() << std::endl;
#else
            catch (const RtAudioError& e) {
                std::cerr << "Error getting default input device info: " << e.what() << std::endl;
#endif
            }
        }

       // If default device is not suitable, fall back to searching for a suitable device
#if RTAUDIO_VERSION_MAJOR >= 6
        for (int i : audio.getDeviceIds()) {
#else
        int count = audio.getDeviceCount();
        for (int i = 0; i < count; i++) {
#endif
            try {
                info = audio.getDeviceInfo(i);
#if !defined(RTAUDIO_VERSION_MAJOR) || RTAUDIO_VERSION_MAJOR < 6
                if (!info.probed) { continue; }
#endif
                std::cout << info.name << " input channels: " << info.inputChannels << std::endl;
                if (info.inputChannels < 1) { continue; }
                selectedDevice = i;
                inputChannels = info.inputChannels;
                std::cout << "Found input device " << selectedDevice << std::endl;
                return;
            }
#if RTAUDIO_VERSION_MAJOR >= 6
            catch (const RtAudioErrorType& e) {
                std::cerr << "Error getting audio device (" << i << ") info: " << audio.getErrorText() << std::endl;
#else
            catch (const RtAudioError& e) {
                std::cerr << "Error getting audio device (" << i << ") info: " << e.what() << std::endl;
#endif
            }
        }

        std::cerr << "No suitable input device found" << std::endl;
    }

    void setupStream() {
#if RTAUDIO_VERSION_MAJOR >= 6
        audio.setErrorCallback(&RtAudioSource::errorCallback);
#endif
        RtAudio::StreamParameters parameters;
        parameters.deviceId = selectedDevice;
        parameters.nChannels = inputChannels;
        parameters.firstChannel = 0;

        RtAudio::StreamOptions options;
        options.flags = RTAUDIO_SCHEDULE_REALTIME;
        options.streamName = "RtAudioSource";

        try {
            audio.openStream(nullptr, &parameters, RTAUDIO_FLOAT32,
                             sampleRate, &framesPerBuffer,
                             &RtAudioSource::callback, this, &options);
        }
#if RTAUDIO_VERSION_MAJOR >= 6
        catch (const RtAudioErrorType& error) {
            std::cerr << "Error opening stream: " << audio.getErrorText() << std::endl;
#else
        catch (const RtAudioError& error) {
            std::cerr << "Error opening stream: " << error.what() << std::endl;
#endif
            return;
        }
    }

    static int callback(void* outputBuffer, void* inputBuffer,
                        unsigned int nBufferFrames, double streamTime,
                        RtAudioStreamStatus status, void* userData)
    {
        RtAudioSource* _this = (RtAudioSource*)userData;
        memcpy(_this->stream_tx.writeBuf, inputBuffer, nBufferFrames * sizeof(dsp::complex_tx));
        _this->stream_tx.swap(nBufferFrames);       
        return 0;
    }

#if RTAUDIO_VERSION_MAJOR >= 6
    static void errorCallback(RtAudioErrorType type, const std::string& errorText) {
        std::cerr << "RtAudio error (" << type << "): " << errorText << std::endl;
    }
#endif
};

#endif // AUDIOSOURCE_H
