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

#if RTAUDIO_VERSION_MAJOR >= 6
            for (int i : audio.getDeviceIds()) {
#else
            int count = audio.getDeviceCount();
            for (int i = 0; i < count; i++) {
#endif
                try {
                    // Get info
                    auto info = audio.getDeviceInfo(i);

#if !defined(RTAUDIO_VERSION_MAJOR) || RTAUDIO_VERSION_MAJOR < 6
                    if (!info.probed) { continue; }
#endif \
    // Check that it has a stereo input
                    if (info.inputChannels < 2) { continue; }

                    if (info.name.find("Microphone") != std::string::npos) {
                        selectedDevice = i;
                        break;
                    }
                }
                catch (const std::exception& e) {
                    flog::error("Error getting audio device ({}) info: {}", i, e.what());
                }
            }


// Set the error callback function
#if RTAUDIO_VERSION_MAJOR >= 6
        audio.setErrorCallback(&RtAudiSource::errorCallback);
#endif

        RtAudio::StreamParameters parameters;
        parameters.deviceId = selectedDevice;

        RtAudio::DeviceInfo deviceInfo = audio.getDeviceInfo(parameters.deviceId);

        parameters.nChannels = 2;
        parameters.firstChannel = 0;
        unsigned int bufferFrames = FRAMES_PER_BUFFER;
        RtAudio::StreamOptions opts;
        opts.flags = RTAUDIO_SCHEDULE_REALTIME;
        opts.streamName = "Rt Audio Source";

        try {
            audio.openStream(nullptr, &parameters, RTAUDIO_FLOAT32, sampleRate, &bufferFrames, &RtAudiSource::callback, this, &opts);
        } catch (const std::exception& e) {
            std::cerr << "Error opening stream: " << e.what() << std::endl;
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
                std::cerr << "RtAudiSource started." << std::endl;
                return true;
            } catch (const std::exception& e) {
                std::cerr << "Error starting stream: " << e.what() << std::endl;
                return false;
            }
        } else {
            std::cerr << "Stream is already running." << std::endl;
            return true; // Akış zaten çalışıyorsa başarılı olduğunu dön
        }
    }

    bool stop()
    {
        if (audio.isStreamRunning()) {
            try {
                audio.stopStream();
                std::cout << "RtAudiSource stopped." << std::endl;
                return true;
            } catch (const std::exception& e) {
                std::cerr << "Error stopping stream: " << e.what() << std::endl;
                return false;
            }
        } else {
            std::cerr << "Stream is not running." << std::endl;
            return true; // Akış zaten durmuşsa başarılı olduğunu dön
        }
    }

private:
    dsp::stream<dsp::complex_t>& stream_buffer;
    double sampleRate;
    RtAudio audio;
    int selectedDevice;

    static const int SAMPLE_RATE = 44100;
    static const int FRAMES_PER_BUFFER = 4096;

    static int callback(void* outputBuffer, void* inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void* userData) {
        if (status) {
            std::cerr << "Stream underflow detected!" << std::endl;
        }

        RtAudiSource* _this = static_cast<RtAudiSource*>(userData);
        memcpy(_this->stream_buffer.writeBuf, inputBuffer, nBufferFrames * sizeof(dsp::complex_t));
        _this->stream_buffer.swap(nBufferFrames);
        return 0;
    }

#if RTAUDIO_VERSION_MAJOR >= 6
    static void errorCallback(RtAudioErrorType type, const std::string& errorText) {
        switch (type) {
        case RTAUDIO_NO_ERROR:
            return;
        case RTAUDIO_WARNING:
        case RTAUDIO_NO_DEVICES_FOUND:
        case RTAUDIO_DEVICE_DISCONNECT:
            std::cerr << "AudioSourceModule Warning: " << errorText << " (" << static_cast<int>(type) << ")" << std::endl;
            break;
        default:
            std::cerr << "AudioSourceModule Error: " << errorText << " (" << static_cast<int>(type) << ")" << std::endl;
            break;
        }
    }
#endif
};
#endif // AUDIOSOURCE_H
