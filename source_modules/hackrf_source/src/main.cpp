#include <utils/flog.h>
#include <module.h>
#include <math.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <config.h>
#include <gui/widgets/stepped_slider.h>
#include <gui/smgui.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <portaudio.h>

#ifndef __ANDROID__
#include <libhackrf/hackrf.h>
#else
#include <android_backend.h>
#include <hackrf.h>
#endif

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "hackrf_source",
    /* Description:     */ "HackRF source module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

const char* AGG_MODES_STR = "Off\0Low\0High\0";
const char* sampleRatesTxt = "20MHz\00016MHz\00010MHz\0008MHz\0005MHz\0004MHz\0002MHz\000";
const int AUDIO_SAMPLE_RATE = 44100;
const unsigned long AUDIO_FRAMES_PER_BUFFER = 256;



const int sampleRates[] = {
    20000000,
    16000000,
    10000000,
    8000000,
    5000000,
    4000000,
    2000000,
};

const int bandwidths[] = {
    1750000,
    2500000,
    3500000,
    5000000,
    5500000,
    6000000,
    7000000,
    8000000,
    9000000,
    10000000,
    12000000,
    14000000,
    15000000,
    20000000,
    24000000,
    28000000,
};

const char* bandwidthsTxt = "1.75MHz\0"
                            "2.5MHz\0"
                            "3.5MHz\0"
                            "5MHz\0"
                            "5.5MHz\0"
                            "6MHz\0"
                            "7MHz\0"
                            "8MHz\0"
                            "9MHz\0"
                            "10MHz\0"
                            "12MHz\0"
                            "14MHz\0"
                            "15MHz\0"
                            "20MHz\0"
                            "24MHz\0"
                            "28MHz\0"
                            "Auto\0";

class HackRFSourceModule : public ModuleManager::Instance {
public:
    HackRFSourceModule(std::string name) {
        this->name = name;

        hackrf_init();

        // Select the last samplerate option
        sampleRate = 2000000;
        srId = 6;

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;

        refresh();

        config.acquire();
        std::string confSerial = config.conf["device"];
        config.release();
        selectBySerial(confSerial);

        sigpath::sourceManager.registerSource("HackRF", &handler);
    }

    ~HackRFSourceModule() {
        stop(this);
        hackrf_exit();
        sigpath::sourceManager.unregisterSource("HackRF");
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
            config.conf["devices"][serial]["txAudioAmp"] = 1.5;
            config.conf["devices"][serial]["bandwidth"] = 16;
            config.conf["devices"][serial]["audioFrequency"] = 440.0;
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
        amplitudeScalingFactor = 1.5;
        bwId = 1;
        audioFrequency = 440.0;

        // Load from config if available and validate
        if (config.conf["devices"][serial].contains("sampleRate")) {
            int psr = config.conf["devices"][serial]["sampleRate"];
            for (int i = 0; i < 7; i++) {
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
        if (config.conf["devices"][serial].contains("txAudioAmp")) {
            amplitudeScalingFactor = config.conf["devices"][serial]["txAudioAmp"];
        }
        if (config.conf["devices"][serial].contains("bandwidth")) {
            bwId = config.conf["devices"][serial]["bandwidth"];
            bwId = std::clamp<int>(bwId, 0, 16);
        }
        if (config.conf["devices"][serial].contains("audioFrequency")) {
            audioFrequency = config.conf["devices"][serial]["audioFrequency"];
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
            _this->amp = true;
            _this->biasT = true;
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

        _this->running = false;
        _this->stream.stopWriter();

        if(_this->ptt)
        {
            hackrf_stop_tx(_this->openDev);
        }
        else
            hackrf_stop_rx(_this->openDev);

        hackrf_error err = (hackrf_error)hackrf_close(_this->openDev);
        if (err != HACKRF_SUCCESS) {
            flog::error("Could not close HackRF {0}: {1}", _this->selectedSerial, hackrf_error_name(err));
        }
        _this->stream.clearWriteStop();
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

        SmGui::LeftLabel("Tx Audio Frequency");
        SmGui::FillWidth();
        if (SmGui::SliderFloatWithSteps(CONCAT("##_hackrf_tx_audio_freq", _this->name), &_this->audioFrequency, 200, 2000, 10, SmGui::FMT_STR_FLOAT_NO_DECIMAL)) {
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["audioFrequency"] = (int)_this->audioFrequency;
            config.release(true);
        }


        SmGui::LeftLabel("Tx Audio Amplitute");
        SmGui::FillWidth();
        if (SmGui::SliderFloatWithSteps(CONCAT("##_hackrf_tx_audio_amp_", _this->name), &_this->amplitudeScalingFactor, 1.5, 3, 0.1, SmGui::FMT_STR_FLOAT_DB_ONE_DECIMAL)) {
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["txAudioAmp"] = _this->amplitudeScalingFactor;
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

    static int send_audio_mic_tx(hackrf_transfer* transfer, std::vector<float> micBuffer) {
        HackRFSourceModule* _this = (HackRFSourceModule*)transfer->tx_ctx;
        // auto modulationIndex = 0.5; // Adjust this value as needed for NFM
        auto modulationIndex = 5.0; // Adjust this value as needed for WFM

        // Check if micBuffer is populated with microphone input
        if (micBuffer.empty()) {
            std::cerr << "Error: micBuffer is empty. Capture microphone input first." << std::endl;
            return -1;
        }
        // Calculate the number of samples to process
        int numSamples = transfer->valid_length / 2;

        for (int sampleIndex = 0; sampleIndex < numSamples; sampleIndex++) {
            // Calculate time in seconds
            double time = (_this->current_tx_sample + sampleIndex) / static_cast<double>(_this->sampleRate);

            // Get microphone data from micBuffer
            double micSample = micBuffer[(sampleIndex % micBuffer.size())];

            double modulatedPhase = 2 * M_PI * _this->freq * time + modulationIndex * micSample;

            // Calculate the in-phase (I) and quadrature (Q) components based on the modulated phase
            double inPhaseComponent = cos(modulatedPhase) * _this->amplitudeScalingFactor;
            double quadratureComponent = sin(modulatedPhase) * _this->amplitudeScalingFactor;

            // Calculate the buffer index
            int bufferIndex = sampleIndex * 2;

            // Pack the I/Q samples into the transfer buffer
            transfer->buffer[bufferIndex] = static_cast<int8_t>(std::clamp(inPhaseComponent * 127, -127.0, 127.0));
            transfer->buffer[bufferIndex + 1] = static_cast<int8_t>(std::clamp(quadratureComponent * 127, -127.0, 127.0));
        }
        _this->current_tx_sample += numSamples;
        return 0;
    }

    static int send_sin_wave_tx(hackrf_transfer* transfer) {
        HackRFSourceModule* _this = (HackRFSourceModule*)transfer->tx_ctx;
        // auto modulationIndex = 0.5; // Adjust this value as needed for NFM
        auto modulationIndex = 5.0; // Adjust this value as needed for WFM

        // Generate the signal to transmit
        for (int sampleIndex = 0; sampleIndex < transfer->valid_length / 2; sampleIndex++) {
            // Calculate time in seconds
            double time = (_this->current_tx_sample + sampleIndex) / static_cast<double>(_this->sampleRate);
            double audioSignal = sin(2 * M_PI * _this->audioFrequency * time);

            double modulatedPhase = 2 * M_PI * _this->freq * time + modulationIndex * audioSignal;

            // Calculate the in-phase (I) and quadrature (Q) components based on the modulated phase
            double inPhaseComponent = cos(modulatedPhase) * _this->amplitudeScalingFactor;
            double quadratureComponent = sin(modulatedPhase) * _this->amplitudeScalingFactor;

            // Calculate the buffer index
            int bufferIndex = sampleIndex * 2;

            // Pack the I/Q samples into the transfer buffer
            transfer->buffer[bufferIndex] = static_cast<int8_t>(std::clamp(inPhaseComponent * 127, -127.0, 127.0));
            transfer->buffer[bufferIndex + 1] = static_cast<int8_t>(std::clamp(quadratureComponent * 127, -127.0, 127.0));
        }

        // Update the current sample index to keep track of samples processed
        _this->current_tx_sample += transfer->valid_length / 2;
        return 0;
    }

    static int callback_tx(hackrf_transfer* transfer) {
        HackRFSourceModule* _this = (HackRFSourceModule*)transfer->tx_ctx;
        _this->send_sin_wave_tx(transfer);
        return 0;
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
    float amplitudeScalingFactor = 1.5;
    float audioFrequency = 440.0;
    int current_tx_sample = 0;

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
