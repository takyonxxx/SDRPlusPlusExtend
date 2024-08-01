#ifndef AUDIOSOURCE_H
#define AUDIOSOURCE_H
#include <iostream>
#include <RtAudio.h>
#include <signal_path/signal_path.h>
#include <stdexcept>

class RtAudiSource {
public:
    RtAudiSource(dsp::stream<dsp::complex_t>& stream_buffer) : stream_buffer(stream_buffer), sampleRate(SAMPLE_RATE), audio()
    {
        if (audio.getDeviceCount() < 1) {
            std::cerr << "No audio devices found!" << std::endl;
            exit(1);
        }

        RtAudio::DeviceInfo info;

        for (int i : audio.getDeviceIds()) {
            try {
                // Get info
                info = audio.getDeviceInfo(i);
                std::cerr << "Mic device " << info.name << std::endl;
                //if (info.inputChannels < 2) { continue; }
                if (info.name.find("Microphone") != std::string::npos) {
                    selectedDevice = i;
                    std::cerr << "Found Mic device " << selectedDevice << std::endl;
                    break;
                }
            }
            catch (const std::exception& e) {
                flog::error("Error getting audio device ({}) info: {}", i, e.what());
            }
        }

        audio.setErrorCallback(&RtAudiSource::errorCallback);
        RtAudio::StreamParameters parameters;
        parameters.deviceId = selectedDevice;
        parameters.nChannels = info.inputChannels;
        parameters.firstChannel = 0;
        unsigned int bufferFrames = FRAMES_PER_BUFFER;
        RtAudio::StreamOptions opts;
        opts.flags = RTAUDIO_SCHEDULE_REALTIME;
        opts.streamName = "Rt Audio Source";

        try {
            audio.openStream(nullptr, &parameters, RTAUDIO_FLOAT32, sampleRate, &bufferFrames, &RtAudiSource::callback, this, &opts);
            std::cout << "Stream opened successfully with device " << selectedDevice << std::endl;
        } catch (const RtAudioErrorType& error) {
            std::cerr << "Error opening stream: " << audio.getErrorText() << std::endl;
        }
    }

    ~RtAudiSource()
    {
        if (audio.isStreamOpen()) audio.closeStream();
    }

    bool start()
    {
        if (!audio.isStreamRunning()) {
            try {
                audio.startStream();
                return true;
            } catch (const std::exception& e) {
                std::cerr << "Error starting stream: " << e.what() << std::endl;
                return false;
            }
        } else {
            return true;
        }
    }

    bool stop()
    {
        if (audio.isStreamRunning()) {
            try {
                audio.stopStream();
                return true;
            } catch (const std::exception& e) {
                std::cerr << "Error stopping stream: " << e.what() << std::endl;
                return false;
            }
        } else {           
            return true;
        }
    }

private:
    dsp::stream<dsp::complex_t>& stream_buffer;
    double sampleRate;
    RtAudio audio;
    int selectedDevice;

    static const int SAMPLE_RATE = 44100;
    static const int FRAMES_PER_BUFFER = 1024;

    static bool is_zero(const dsp::complex_t& value) {
        return value.re == 0 && value.im == 0;
    }

    static bool is_nan(const dsp::complex_t& value) {
        return std::isnan(value.re) || std::isnan(value.im);
    }

    static int callback(void* outputBuffer, void* inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void* userData) {
        RtAudiSource* _this = static_cast<RtAudiSource*>(userData);
        memcpy(_this->stream_buffer.writeBuf, inputBuffer, nBufferFrames * sizeof(dsp::complex_t));
        _this->stream_buffer.swap(nBufferFrames);
        return 0;
    }

#if RTAUDIO_VERSION_MAJOR >= 6
    static void errorCallback(RtAudioErrorType type, const std::string& errorText) {
        switch (type) {
        case RTAUDIO_NO_ERROR:
            std::cout << "AudioSourceModule: No error" << std::endl;
            return;
        case RTAUDIO_WARNING:
            std::cout << "AudioSourceModule Warning: " << errorText << std::endl;
            break;
        case RTAUDIO_NO_DEVICES_FOUND:
            std::cout << "AudioSourceModule Error: No devices found - " << errorText << std::endl;
            break;
        case RTAUDIO_DEVICE_DISCONNECT:
            std::cout << "AudioSourceModule Error: Device disconnected - " << errorText << std::endl;
            break;
        case RTAUDIO_INVALID_DEVICE:
            std::cout << "AudioSourceModule Error: Invalid device - " << errorText << std::endl;
            break;
        case RTAUDIO_INVALID_PARAMETER:
            std::cout << "AudioSourceModule Error: Invalid parameter - " << errorText << std::endl;
            break;
        case RTAUDIO_INVALID_USE:
            std::cout << "AudioSourceModule Error: Invalid use - " << errorText << std::endl;
            break;
        case RTAUDIO_DRIVER_ERROR:
            std::cout << "AudioSourceModule Error: Driver error - " << errorText << std::endl;
            break;
        case RTAUDIO_SYSTEM_ERROR:
            std::cout << "AudioSourceModule Error: System error - " << errorText << std::endl;
            break;
        case RTAUDIO_THREAD_ERROR:
            std::cout << "AudioSourceModule Error: Thread error - " << errorText << std::endl;
            break;
        default:
            std::cout << "AudioSourceModule Error: Unknown error type - " << errorText << " (" << static_cast<int>(type) << ")" << std::endl;
            break;
        }
    }
#endif
};
#endif // AUDIOSOURCE_H
