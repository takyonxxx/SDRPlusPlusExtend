#include "constants.h"

class HackRFSinkModule : public ModuleManager::Instance {
public:

    HackRFSinkModule(std::string name):
        name(name),
        enabled(true),
        sampleRate(2000000),
        audioSampleRate(44100),
        interpolation(48 * (sampleRate / _MHZ(2))),
        freq(100e6),
        srId(7),
        bwId(0),
        amp(true),
        tx_vga(0),
        running(false)
    {
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

        this->tb = gr::make_top_block("HackRfSinkTopBlock");

        this->rational_resampler_xxx_0 = gr::filter::rational_resampler_ccf::make(this->interpolation, 1);
        this->blocks_multiply_const_vxx_0 = gr::blocks::multiply_const_ff::make(4);
        this->audio_source_0 = gr::audio::source::make(this->audioSampleRate, "", true);
        this->analog_frequency_modulator_fc_0 = gr::analog::frequency_modulator_fc::make(1.5);

        sigpath::sourceManager.registerSource("HackRFSink", &handler);
        flog::info("{} Sample rate: {} Hz, Frequency: {} Hz BandWidth: {} Hz", this->name, this->sampleRate, this->freq, this->bandwidthIdToBw(this->bwId));
    }

     ~HackRFSinkModule() {
        stop(this);
        this->tb.reset();
        this->rational_resampler_xxx_0.reset();
        this->blocks_multiply_const_vxx_0.reset();
        this->audio_source_0.reset();
        this->analog_frequency_modulator_fc_0.reset();
        this->soapy_hackrf_sink_0.reset();        
        sigpath::sourceManager.unregisterSource("HackRFSink");
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

        try {
            // Initialize the HackRF library
            int result = hackrf_init();
            if (result != HACKRF_SUCCESS) {
                flog::error("Failed to initialize HackRF: {}", hackrf_error_name(static_cast<hackrf_error>(result)));
                return;
            }

            // Get the list of HackRF devices
            hackrf_device_list_t* devices = hackrf_device_list();
            if (devices->devicecount < 1) {
                flog::error("No HackRF devices found");
                hackrf_device_list_free(devices);
                hackrf_exit();
                devList.push_back("No HackRF devices found");
                devListTxt += (char*)("No HackRF devices found");
                devListTxt += '\0';
                return;
            }

            devList.clear();
            devListTxt = "";         

            // Iterate through the list of devices and retrieve their serial numbers
            for (int i = 0; i < devices->devicecount; ++i) {
                if (devices->serial_numbers[i] != nullptr) {
                    devList.push_back(devices->serial_numbers[i]);
                    devListTxt += (char*)(devices->serial_numbers[i] + 16);
                    devListTxt += '\0';
                }
            }

            // Free the device list and deinitialize the library
            hackrf_device_list_free(devices);
            hackrf_exit();
        } catch (const std::exception& e) {
            flog::error("Exception caught in refresh: {}", e.what());
        }
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
            config.conf["devices"][serial]["amp"] = true;
            config.conf["devices"][serial]["txVgaGain"] = 47;
            config.conf["devices"][serial]["bandwidth"] = 0;

        }
        config.release(created);

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
        if (config.conf["devices"][serial].contains("amp")) {
            amp = config.conf["devices"][serial]["amp"];
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
        HackRFSinkModule* _this = (HackRFSinkModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        flog::info("HackRFSinkModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        HackRFSinkModule* _this = (HackRFSinkModule*)ctx;
        flog::info("HackRFSinkModule '{0}': Menu Deselect!", _this->name);
    }

    int bandwidthIdToBw(int id) {
        if (id == 16) { return hackrf_compute_baseband_filter_bw(sampleRate); }
        return bandwidths[id];
    }

    static void menuHandler(void* ctx) {
        HackRFSinkModule* _this = (HackRFSinkModule*)ctx;
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
            if(_this->running)
            {
                _this->soapy_hackrf_sink_0->set_sample_rate(0, _this->sampleRate);
            }
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
                _this->soapy_hackrf_sink_0->set_bandwidth(0, _this->bandwidthIdToBw(_this->bwId));
            }
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["bandwidth"] = _this->bwId;
            config.release(true);
        }

        SmGui::LeftLabel("Tx VGA Gain");
        SmGui::FillWidth();
        if (SmGui::SliderInt(CONCAT("##_hackrf_tx_vga_", _this->name), &_this->tx_vga, 0, 47, SmGui::FMT_STR_INT_DB)) {
            if (_this->running) {
                _this->soapy_hackrf_sink_0->set_gain(0, "VGA", std::min(std::max(HACKRF_TX_VGA_MAX_DB, 0), _this->tx_vga));
            }
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["txVgaGain"] = (int)_this->tx_vga;
            config.release(true);
        }       

        if (SmGui::Checkbox(CONCAT("Amp Enabled##_hackrf_amp_", _this->name), &_this->amp)) {
            if (_this->running) {
                _this->soapy_hackrf_sink_0->set_gain(0, "AMP", _this->amp);
            }
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["amp"] = _this->amp;
            config.release(true);
        }
    }

    static void start(void* ctx) {
        HackRFSinkModule* _this = (HackRFSinkModule*)ctx;
        if (_this->running) { return; }        

        if (_this->selectedSerial == "") {
            flog::error("Tried to start HackRF sink with empty serial");
            return;
        }

        std::string dev = "hackrf=0";
        std::string stream_args = "";
        std::vector<std::string> tune_args = {""};
        std::vector<std::string> settings = {""};

        try {
            _this->soapy_hackrf_sink_0 = gr::soapy::sink::make(
                "hackrf",
                "fc32",
                1,
                dev,
                stream_args,
                tune_args,
                settings
                );
        } catch (const std::exception& e) {
            flog::error("Exception caught while creating HackRf sink: {}", e.what());
            return;
        }

        _this->interpolation = 48 * (_this->sampleRate / _MHZ(2));
        _this->soapy_hackrf_sink_0->set_sample_rate(0, _this->sampleRate);
        _this->soapy_hackrf_sink_0->set_frequency(0, _this->freq);
        _this->soapy_hackrf_sink_0->set_bandwidth(0, _this->bandwidthIdToBw(_this->bwId));
        _this->soapy_hackrf_sink_0->set_gain(0, "AMP", _this->amp);
        _this->soapy_hackrf_sink_0->set_gain(0, "VGA", std::min(std::max(HACKRF_TX_VGA_MAX_DB, 0), _this->tx_vga));

        _this->tb->lock();        
        _this->tb->connect((const gr::block_sptr&)_this->audio_source_0, 0, (const gr::block_sptr&)_this->blocks_multiply_const_vxx_0, 0);
        _this->tb->connect((const gr::block_sptr&)_this->blocks_multiply_const_vxx_0, 0, (const gr::block_sptr&)_this->analog_frequency_modulator_fc_0, 0);
        _this->tb->connect((const gr::block_sptr&)_this->analog_frequency_modulator_fc_0, 0, (const gr::block_sptr&)_this->rational_resampler_xxx_0, 0);
        _this->tb->connect((const gr::block_sptr&)_this->rational_resampler_xxx_0, 0, (const gr::block_sptr&)_this->soapy_hackrf_sink_0, 0);
        _this->tb->unlock();

        try {
            _this->tb->start();
            core::setInputSampleRate(_this->sampleRate);
             _this->running = true;
        } catch (const std::exception& e) {
            flog::error("Exception caught while starting hackrf sink top block: {}", e.what());
            return;
        }

        flog::info("HackRFSinkModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        HackRFSinkModule* _this = (HackRFSinkModule*)ctx;

        if (!_this->running) { return; }

        try {
            _this->tb->stop();
            _this->tb->wait();
            _this->tb->lock();
            _this->tb->disconnect_all();
            _this->tb->unlock();
            _this->soapy_hackrf_sink_0.reset();
        } catch (const std::exception& e) {
            flog::error("Exception caught while stopping hackrf sink top block: {}", e.what());
        }

        _this->running = false;
        _this->stream.stopWriter();
        _this->stream.clearWriteStop();
        flog::info("HackRFSinkModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        HackRFSinkModule* _this = (HackRFSinkModule*)ctx;
        _this->freq = freq;

        if (_this->running) {
            _this->soapy_hackrf_sink_0->set_frequency(0, freq);
        }        

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

        flog::info("HackRFSinkModule '{0}': Tune: {1}!", _this->name, display_freq_str);
    }

    std::string name;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    int sampleRate;
    int audioSampleRate;
    double interpolation;
    SourceManager::SourceHandler handler;
    bool running = false;
    double freq;
    std::string selectedSerial = "";
    int devId = 0;
    int srId = 0;
    int bwId = 16;
    bool amp = false;
    int tx_vga = 0;

    gr::top_block_sptr tb;
    gr::soapy::sink::sptr soapy_hackrf_sink_0;
    gr::filter::rational_resampler_ccf::sptr rational_resampler_xxx_0;
    gr::blocks::multiply_const_ff::sptr blocks_multiply_const_vxx_0;
    gr::audio::source::sptr audio_source_0;
    gr::analog::frequency_modulator_fc::sptr analog_frequency_modulator_fc_0;

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
    config.setPath(core::args["root"].s() + "/hackrf_sink_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new HackRFSinkModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (HackRFSinkModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}
