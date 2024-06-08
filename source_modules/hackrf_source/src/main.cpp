#include "hackrfsourcemodule.h"
#include "constants.h"
#include <portaudio.h>

class PortAudioSource {
public:
    PortAudioSource(dsp::stream<dsp::complex_t>& stream_buffer) : stream_buffer(stream_buffer), stream(nullptr), isRecording(false)
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

        inputParameters.channelCount = 2;
        inputParameters.sampleFormat = paFloat32;
        inputParameters.suggestedLatency = deviceInfo->defaultHighInputLatency;
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
    dsp::stream<dsp::complex_t>& stream_buffer;

    static const int SAMPLE_RATE = 44100;
    static const int FRAMES_PER_BUFFER = 256;

    static int paCallback(const void* inputBuffer, void* outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void* userData) {
        PortAudioSource* _this = static_cast<PortAudioSource*>(userData);       

        // volk_8i_s32f_convert_32f((float*)_this->stream_buffer.writeBuf, in, 128.0f, framesPerBuffer);
        // if (!_this->stream_buffer.swap(framesPerBuffer)) { return -1; }

        memcpy(_this->stream_buffer.writeBuf, inputBuffer, framesPerBuffer * sizeof(float));
        _this->stream_buffer.swap(framesPerBuffer);

        return paContinue;
    }

};

class HackRFSourceModule : public ModuleManager::Instance {
public:

    HackRFSourceModule(std::string name):
        name(name)
    {
        this->name = name;

        hackrf_init();

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;

        portAudioSource = new PortAudioSource(stream);

        refresh();

        config.acquire();
        std::string confSerial = config.conf["device"];
        config.release();
        selectBySerial(confSerial);
        sigpath::sourceManager.registerSource("HackRFSource", &handler);
    }

    ~HackRFSourceModule() {

        if(portAudioSource)
        {
            portAudioSource->stop();
            delete portAudioSource;
        }

        stop(this);
        hackrf_exit();
        sigpath::sourceManager.unregisterSource("HackRF");
    }

    void startRecording() {

        if (!portAudioSource->start()) {
            std::cerr << "Error start portAudioSource!" << std::endl;
        }
    }

    void stopRecording() {
        if (!portAudioSource->stop()) {
            std::cerr << "Error stop portAudioSource!" << std::endl;
        }
    }

    std::string findProjectFolder(const std::string& marker) {
        fs::path currentDir = fs::current_path();
        while (currentDir.has_parent_path()) {
            if (fs::exists(currentDir / marker)) {
                return currentDir.string();
            }
            currentDir = currentDir.parent_path();
        }

        std::cerr << "Failed to find the project root folder." << std::endl;
        return std::string();
    }

    void postInit() {}

    void enable() {
        enabled = true;
    }

    void disable() {
        enabled = false;
    }

    bool isEnabled() {
        return enabled;
    }

    void refresh() {

        devList.clear();
        devListTxt = "";        

#ifndef __ANDROID__
        uint64_t serials[256];
        hackrf_device_list_t* _devList = hackrf_device_list();

        for (int i = 0; i < _devList->devicecount; i++) {
            // Skip devices that are in use
            if (_devList->serial_numbers[i] == NULL) { continue; }

            // Save the device serial number
            devList.push_back(_devList->serial_numbers[i]);
            devListTxt += (char*)(_devList->serial_numbers[i] + 16);
            devListTxt += '\0';
        }

        if(devListTxt.empty())
        {
            devList.push_back("No HackRF devices found");
            devListTxt += (char*)("No HackRF devices found");
            devListTxt += '\0';
        }

        hackrf_device_list_free(_devList);
#else
        int vid, pid;
        devFd = backend::getDeviceFD(vid, pid, backend::HACKRF_VIDPIDS);
        if (devFd < 0) { return; }
        std::string fakeName = "HackRF USB";
        devList.push_back("fake_serial");
        devListTxt += fakeName;
        devListTxt += '\0';
#endif

    }

    void selectFirst() {
        if (devList.size() != 0) {
            selectBySerial(devList[0]);
            return;
        }
        selectedSerial = "";
    }

    void selectBySerial(std::string serial) {
        if (std::find(devList.begin(), devList.end(), serial) == devList.end()) {
            selectFirst();
            return;
        }

        bool created = false;
        config.acquire();
        if (!config.conf["devices"].contains(serial)) {
            config.conf["devices"][serial]["sampleRate"] = 2000000;
            config.conf["devices"][serial]["biasT"] = false;
            config.conf["devices"][serial]["amp"] = false;
            config.conf["devices"][serial]["ptt"] = false;
            config.conf["devices"][serial]["lnaGain"] = 40;
            config.conf["devices"][serial]["vgaGain"] = 40;
            config.conf["devices"][serial]["txVgaGain"] = 47;
            config.conf["devices"][serial]["bandwidth"] = 16;
        }
        config.release(created);

        // Set default values
        srId = 0;
        sampleRate = 2000000;
        biasT = false;
        amp = false;
        ptt = false;
        lna = 0;
        vga = 0;
        tx_vga = 0;
        bwId = 1;

        // Load from config if available and validate
        if (config.conf["devices"][serial].contains("sampleRate")) {
            int psr = config.conf["devices"][serial]["sampleRate"];
            for (int i = 0; i < 8; i++) {
                if (sampleRates[i] == psr) {
                    sampleRate = psr;
                    srId = i;
                }
            }
        }
        if (config.conf["devices"][serial].contains("biasT")) {
            biasT = config.conf["devices"][serial]["biasT"];
        }
        if (config.conf["devices"][serial].contains("amp")) {
            amp = config.conf["devices"][serial]["amp"];
        }
        if (config.conf["devices"][serial].contains("ptt")) {
            ptt = config.conf["devices"][serial]["ptt"];
        }
        if (config.conf["devices"][serial].contains("lnaGain")) {
            lna = config.conf["devices"][serial]["lnaGain"];
        }
        if (config.conf["devices"][serial].contains("vgaGain")) {
            vga = config.conf["devices"][serial]["vgaGain"];
        }
        if (config.conf["devices"][serial].contains("txVgaGain")) {
            tx_vga = config.conf["devices"][serial]["txVgaGain"];
        }
        if (config.conf["devices"][serial].contains("bandwidth")) {
            bwId = config.conf["devices"][serial]["bandwidth"];
            bwId = std::clamp<int>(bwId, 0, 16);
        }        

        selectedSerial = serial;
    }

private:
    static void menuSelected(void* ctx) {
        HackRFSourceModule* _this = (HackRFSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        flog::info("HackRFSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        HackRFSourceModule* _this = (HackRFSourceModule*)ctx;
        flog::info("HackRFSourceModule '{0}': Menu Deselect!", _this->name);
    }

    int bandwidthIdToBw(int id) {
        if (id == 16) { return hackrf_compute_baseband_filter_bw(sampleRate); }
        return bandwidths[id];
    }

    static void start(void* ctx) {
        HackRFSourceModule* _this = (HackRFSourceModule*)ctx;

        if (_this->running) { return; }

        if (_this->selectedSerial == "") {
            flog::error("Tried to start HackRF source with empty serial");
            return;
        }

#ifndef __ANDROID__
        hackrf_error err = (hackrf_error)hackrf_open_by_serial(_this->selectedSerial.c_str(), &_this->openDev);
#else
        hackrf_error err = (hackrf_error)hackrf_open_by_fd(_this->devFd, &_this->openDev);
#endif
        if (err != HACKRF_SUCCESS) {
            flog::error("Could not open HackRF {0}: {1}", _this->selectedSerial, hackrf_error_name(err));
            return;
        }

        _this->current_tx_sample = 0;
        if(_this->ptt)
        {
            _this->biasT = true;
            _this->startRecording();
        }
        else
        {
            _this->amp = false;
            _this->biasT = false;
        }

        hackrf_set_sample_rate(_this->openDev, _this->sampleRate);
        hackrf_set_baseband_filter_bandwidth(_this->openDev, _this->bandwidthIdToBw(_this->bwId));
        hackrf_set_freq(_this->openDev, _this->freq);
        hackrf_set_antenna_enable(_this->openDev, _this->biasT);
        hackrf_set_amp_enable(_this->openDev, _this->amp);
        hackrf_set_lna_gain(_this->openDev, _this->lna);
        hackrf_set_vga_gain(_this->openDev, _this->vga);
        hackrf_set_txvga_gain(_this->openDev, _this->tx_vga);

        if(_this->ptt)
        {
            hackrf_set_baseband_filter_bandwidth(_this->openDev,  _this->sampleRate*0.75);
            hackrf_start_tx(_this->openDev, callback_tx, _this);
        }
        else
            hackrf_start_rx(_this->openDev, callback_rx, _this);

        _this->running = true;
        flog::info("HackRFSourceModule '{0} {1}': Start!", _this->name, _this->ptt);
    }

    static void stop(void* ctx) {
        HackRFSourceModule* _this = (HackRFSourceModule*)ctx;

        if (!_this->running) { return; }        
        _this->stream.stopWriter();

        if(_this->ptt)
        {
            hackrf_stop_tx(_this->openDev);
            _this->stopRecording();
        }
        else
            hackrf_stop_rx(_this->openDev);

        hackrf_error err = (hackrf_error)hackrf_close(_this->openDev);
        if (err != HACKRF_SUCCESS) {
            flog::error("Could not close HackRF {0}: {1}", _this->selectedSerial, hackrf_error_name(err));
        }

        _this->stream.clearWriteStop();
        _this->running = false;

        flog::info("HackRFSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        HackRFSourceModule* _this = (HackRFSourceModule*)ctx;
        if (_this->running) {
            hackrf_set_freq(_this->openDev, freq);
        }
        _this->freq = freq;

        double display_freq; // Change to double for decimal precision
        std::string unit;

        if (freq < 1e3) {
            display_freq = freq;
            unit = "Hz";
        } else if (freq < 1e6) {
            display_freq = freq / 1e3;
            unit = "kHz";
        } else if (freq < 1e9) {
            display_freq = freq / 1e6;
            unit = "MHz";
        } else {
            display_freq = freq / 1e9;
            unit = "GHz";
        }

        std::stringstream stream;
        stream << std::fixed << std::setprecision(1) << display_freq; // Set precision for one decimal place
        std::string display_freq_str = stream.str();

        // Append unit
        display_freq_str += " " + unit;

        flog::info("HackRFSourceModule '{0}': Tune: {1}!", _this->name, display_freq_str);
    }

    static void menuHandler(void* ctx) {
        HackRFSourceModule* _this = (HackRFSourceModule*)ctx;

        if (_this->running) { SmGui::BeginDisabled(); }
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_hackrf_dev_sel_", _this->name), &_this->devId, _this->devListTxt.c_str())) {
            _this->selectBySerial(_this->devList[_this->devId]);
            config.acquire();
            config.conf["device"] = _this->selectedSerial;
            config.release(true);
        }

        if (SmGui::Combo(CONCAT("##_hackrf_sr_sel_", _this->name), &_this->srId, sampleRatesTxt)) {
            _this->sampleRate = sampleRates[_this->srId];
            core::setInputSampleRate(_this->sampleRate);
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["sampleRate"] = _this->sampleRate;
            config.release(true);
        }

        SmGui::SameLine();
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Button(CONCAT("Refresh##_hackrf_refr_", _this->name))) {
            _this->refresh();
            _this->selectBySerial(_this->selectedSerial);
            core::setInputSampleRate(_this->sampleRate);
        }

        if (_this->running) { SmGui::EndDisabled(); }

        SmGui::LeftLabel("Bandwidth");
        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("##_hackrf_bw_sel_", _this->name), &_this->bwId, bandwidthsTxt)) {
            if (_this->running) {
                hackrf_set_baseband_filter_bandwidth(_this->openDev, _this->bandwidthIdToBw(_this->bwId));
            }
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["bandwidth"] = _this->bwId;
            config.release(true);
        }

        SmGui::LeftLabel("LNA Gain");
        SmGui::FillWidth();
        if (SmGui::SliderFloatWithSteps(CONCAT("##_hackrf_lna_", _this->name), &_this->lna, 0, 40, 1, SmGui::FMT_STR_FLOAT_DB_NO_DECIMAL)) {
            if (_this->running) {
                hackrf_set_lna_gain(_this->openDev, _this->lna);
            }
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["lnaGain"] = (int)_this->lna;
            config.release(true);
        }

        SmGui::LeftLabel("VGA Gain");
        SmGui::FillWidth();
        if (SmGui::SliderFloatWithSteps(CONCAT("##_hackrf_vga_", _this->name), &_this->vga, 0, 62, 1, SmGui::FMT_STR_FLOAT_DB_NO_DECIMAL)) {
            if (_this->running) {
                hackrf_set_vga_gain(_this->openDev, _this->vga);
            }
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["vgaGain"] = (int)_this->vga;
            config.release(true);
        }

        SmGui::LeftLabel("Tx VGA Gain");
        SmGui::FillWidth();
        if (SmGui::SliderFloatWithSteps(CONCAT("##_hackrf_tx_vga_", _this->name), &_this->tx_vga, 0, 47, 1, SmGui::FMT_STR_FLOAT_DB_NO_DECIMAL)) {
            if (_this->running) {
                hackrf_set_txvga_gain(_this->openDev, _this->tx_vga);
            }
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["txVgaGain"] = (int)_this->tx_vga;
            config.release(true);
        }

        if (SmGui::Checkbox(CONCAT("Ptt Enabled - Tx Mode##_hackrf_ptt_", _this->name), &_this->ptt)) {

            if (_this->running) {
                _this->stop(ctx);
            }

            _this->start(ctx);

            config.acquire();
            config.conf["devices"][_this->selectedSerial]["ptt"] = _this->ptt;
            config.release(true);
        }

        if (SmGui::Checkbox(CONCAT("Bias-T##_hackrf_bt_", _this->name), &_this->biasT)) {
            if (_this->running) {
                hackrf_set_antenna_enable(_this->openDev, _this->biasT);
            }
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["biasT"] = _this->biasT;
            config.release(true);
        }

        if (SmGui::Checkbox(CONCAT("Amp Enabled##_hackrf_amp_", _this->name), &_this->amp)) {
            if (_this->running) {
                hackrf_set_amp_enable(_this->openDev, _this->amp);
            }
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["amp"] = _this->amp;
            config.release(true);
        }
    }       

    static int callback_rx(hackrf_transfer* transfer) {
        HackRFSourceModule* _this = (HackRFSourceModule*)transfer->rx_ctx;
        volk_8i_s32f_convert_32f((float*)_this->stream.writeBuf, (int8_t*)transfer->buffer, 128.0f, transfer->valid_length);
        if (!_this->stream.swap(transfer->valid_length / 2)) { return -1; }
        return 0;
    }

    void apply_modulation(int8_t* buffer, uint32_t length) {

        double modulationIndex = 5.0;
        double amplitudeScalingFactor = 1.5;
        double audioSampleRate = 44100.0;
        double newSampleRate = sampleRate / 60.0;
        double interpolation = sampleRate / newSampleRate;
        int decimation = 1;

        int size = BUF_LEN / 2;

        std::vector<float> float_buffer = stream.readBufferToVector();
        while (float_buffer.size() < size) {
            std::vector<float> additional_data = stream.readBufferToVector();
            float_buffer.insert(float_buffer.end(), additional_data.begin(), additional_data.end());
        }

        // std::cout << "Mic Size : " << float_buffer.size() << std::endl;
        // for (auto sample : float_buffer) {
        //     std::cout << sample << std::endl;
        // }

        float sensitivity = modulationIndex; //* (freq / audioSampleRate);
        std::vector<float> modulated_audio = frequency_modulator(float_buffer, sensitivity);

        float cutoff_freq = 1.0 * 44100; // Set cutoff frequency to Nyquist frequency
        std::vector<float> filtered_audio = low_pass_filter(modulated_audio, cutoff_freq, 44100);

        // Apply amplitude scaling
        std::vector<float> multiplied_audio = multiply_const(filtered_audio, amplitudeScalingFactor);

        // Resample audio
        std::vector<float> resampled_audio = rational_resampler(multiplied_audio, interpolation, decimation);

        for (size_t i = 0; i < length / 2 && i * 2 + 1 < resampled_audio.size(); ++i) {
            buffer[i * 2] = static_cast<int8_t>(std::max(std::min(resampled_audio[i * 2] * 127.0f, 127.0f), -127.0f));
            buffer[i * 2 + 1] = static_cast<int8_t>(std::max(std::min(resampled_audio[i * 2 + 1] * 127.0f, 127.0f), -127.0f));
        }

        // for (size_t i = 0; i < length && i < resampled_audio.size(); ++i) {
        //     buffer[i] = static_cast<int8_t>(std::max(std::min(resampled_audio[i] * 127.0f, 127.0f), -127.0f));
        // }
    }

    int send_mic_tx(int8_t* buffer, uint32_t length) {
        apply_modulation(buffer, length);
        return 0;
    }

    static int callback_tx(hackrf_transfer* transfer) {
        HackRFSourceModule* _this = (HackRFSourceModule*)transfer->tx_ctx;
        return _this->send_mic_tx((int8_t *)transfer->buffer, transfer->valid_length);
    }

    std::string name;
    hackrf_device* openDev;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    int sampleRate;
    SourceManager::SourceHandler handler;
    bool running = false;
    double freq;
    std::string selectedSerial = "";
    int devId = 0;
    int srId = 0;
    int bwId = 16;
    bool biasT = false;
    bool amp = false;
    bool ptt = false;
    float lna = 0;
    float vga = 0;
    float tx_vga = 0;
    int current_tx_sample = 0;

    PortAudioSource *portAudioSource;

#ifdef __ANDROID__
    int devFd = -1;
#endif

    std::vector<std::string> devList;
    std::string devListTxt;
};


MOD_EXPORT void _INIT_() {
    json def = json({});
    def["devices"] = json({});
    def["device"] = "";
    config.setPath(core::args["root"].s() + "/hackrf_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new HackRFSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (HackRFSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}
