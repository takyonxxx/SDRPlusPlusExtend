#ifndef AUDIOSOURCE_H
#define AUDIOSOURCE_H
#include <iostream>
#include <portaudio.h>
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

    for (int deviceId : audio.getDeviceIds()) {
        try {
            // Cihaz bilgilerini al
            auto info = audio.getDeviceInfo(deviceId);
            std::cout << "DeviceId: " << deviceId << ", Name: " << info.name << std::endl;

            if (info.name.find("MacBook Pro Microphone") != std::string::npos) {
                std::cout << "MacBook Pro Microphone bulundu!" << std::endl;
                selectedDevice = deviceId;
                break; // İstenen cihazı bulduktan sonra döngüden çık
            }
        } catch (const std::exception& e) {
            flog::error("Audio cihazı ({}) bilgisi alınırken hata oluştu: {}", deviceId, e.what());
        }
    }

// Set the error callback function
#if RTAUDIO_VERSION_MAJOR >= 6
        audio.setErrorCallback(&RtAudiSource::errorCallback);
#endif

        RtAudio::StreamParameters parameters;
        parameters.deviceId = selectedDevice;

        RtAudio::DeviceInfo deviceInfo = audio.getDeviceInfo(parameters.deviceId);
        std::cout << "Using audio device: " << deviceInfo.name << std::endl;

        parameters.nChannels = 1;
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


class PortAudioSource {
public:
    PortAudioSource(dsp::stream<dsp::complex_t>& stream_buffer) : stream_buffer(stream_buffer), stream(nullptr)
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

        inputParameters.channelCount = 1;
        inputParameters.sampleFormat = paFloat32;
        inputParameters.suggestedLatency = deviceInfo->defaultLowInputLatency;
        inputParameters.hostApiSpecificStreamInfo = nullptr;

        err = Pa_OpenStream(&stream, &inputParameters, nullptr, SAMPLE_RATE, FRAMES_PER_BUFFER, paClipOff, &PortAudioSource::paCallback, this);
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
        std::cerr << "PortAudioSource started." << std::endl;
        return true;
    }

    bool stop()
    {
        if (stream) {
            Pa_StopStream(stream);
        }
        std::cout << "PortAudioSource stopped." << std::endl;        
        return true;
    }

private:
    PaStream* stream;
    PaError err;    
    dsp::stream<dsp::complex_t>& stream_buffer;

    static const int SAMPLE_RATE = 44100;
    static const int FRAMES_PER_BUFFER = 4096;

    static int paCallback(const void* inputBuffer, void* outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void* userData) {
        PortAudioSource* _this = static_cast<PortAudioSource*>(userData);

        // volk_8i_s32f_convert_32f((float*)_this->stream_buffer.writeBuf, in, 128.0f, framesPerBuffer);
        // if (!_this->stream_buffer.swap(framesPerBuffer)) { return -1; }

        memcpy(_this->stream_buffer.writeBuf, inputBuffer, framesPerBuffer * sizeof(dsp::complex_t));
        _this->stream_buffer.swap(framesPerBuffer);

        return paContinue;
    }

};

#endif // AUDIOSOURCE_H
