#include "Constants.h"
#include "FrequencyModulator.h"
#include "RationalResampler.h"
#include "AudioSource.h"
#include <algorithm>
#include <vector>
#include <cstddef>

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

        rtAudioSource = new RtAudiSource(stream);
        // portAudioSource = new PortAudioSource(stream);

        refresh();

        config.acquire();
        std::string confSerial = config.conf["device"];
        config.release();
        selectBySerial(confSerial);
        sigpath::sourceManager.registerSource("HackRFSource", &handler);
    }

    ~HackRFSourceModule() {

        if(rtAudioSource)
        {
            rtAudioSource->stop();
            delete rtAudioSource;
        }

        stop(this);
        hackrf_exit();
        sigpath::sourceManager.unregisterSource("HackRF");
    }

    void startRecording() {

        if(rtAudioSource)
        {
            if (!rtAudioSource->start()) {
                std::cerr << "Error start AudioSource!" << std::endl;
            }
        }       
    }

    void stopRecording() {       
        if(rtAudioSource)
        {
            if (!rtAudioSource->stop()) {
                std::cerr << "Error stop AudioSource!" << std::endl;
            }
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
            config.conf["devices"][serial]["sampleRate"] = 20000000;
            config.conf["devices"][serial]["biasT"] = false;
            config.conf["devices"][serial]["amp"] = false;
            config.conf["devices"][serial]["ptt"] = false;
            config.conf["devices"][serial]["lnaGain"] = 40;
            config.conf["devices"][serial]["vgaGain"] = 40;
            config.conf["devices"][serial]["txVgaGain"] = 47;
            config.conf["devices"][serial]["bandwidth"] = 16;
            config.conf["devices"][serial]["amplitude"] = 5.0;
            config.conf["devices"][serial]["filter_size"] = 0.0;
            config.conf["devices"][serial]["modulation_index"] = 5.0;
            config.conf["devices"][serial]["interpolation"] = 48;
        }
        config.release(created);

        // Set default values
        srId = 0;
        sampleRate = 20000000;
        biasT = false;
        amp = false;
        ptt = false;
        lna = 0;
        vga = 0;
        tx_vga = 0;
        amplitude = 5.0;
        filter_size = 0.0;
        modulation_index = 5.0;
        interpolation = 48;
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
        if (config.conf["devices"][serial].contains("amplitude")) {
            amplitude = config.conf["devices"][serial]["amplitude"];
        }
        if (config.conf["devices"][serial].contains("filter_size")) {
            filter_size = config.conf["devices"][serial]["filter_size"];
        }
        if (config.conf["devices"][serial].contains("modulation_index")) {
            modulation_index = config.conf["devices"][serial]["modulation_index"];
        }
        if (config.conf["devices"][serial].contains("interpolation")) {
            interpolation = config.conf["devices"][serial]["interpolation"];
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

        if (_this->rx_running) { return; }

        if (_this->selectedSerial == "") {
            flog::error("Tried to start HackRF source with empty serial");
            return;
        }

        hackrf_error err = (hackrf_error)hackrf_open_by_serial(_this->selectedSerial.c_str(), &_this->openDev);
        if (err != HACKRF_SUCCESS) {
            flog::error("Could not open HackRF {0}: {1}", _this->selectedSerial, hackrf_error_name(err));
            return;
        }

        _this->amp = false;
        _this->biasT = false;

        hackrf_set_sample_rate(_this->openDev, _this->sampleRate);
        hackrf_set_baseband_filter_bandwidth(_this->openDev, _this->bandwidthIdToBw(_this->bwId));
        hackrf_set_freq(_this->openDev, _this->freq);
        hackrf_set_antenna_enable(_this->openDev, _this->biasT);
        hackrf_set_amp_enable(_this->openDev, _this->amp);
        hackrf_set_lna_gain(_this->openDev, _this->lna);
        hackrf_set_vga_gain(_this->openDev, _this->vga);

        hackrf_start_rx(_this->openDev, callback_rx, _this);

        _this->rx_running = true;
        flog::info("HackRFSourceModule '{0}': Started Rx Mode!", _this->name);
    }

    static void stop(void* ctx) {
        HackRFSourceModule* _this = (HackRFSourceModule*)ctx;
        hackrf_error err;

        if (!_this->rx_running) { return; }
        if (!_this->tx_running) { return; }
        _this->stream.stopWriter();

        hackrf_stop_rx(_this->openDev);      

        err = (hackrf_error)hackrf_close(_this->openDev);
        if (err != HACKRF_SUCCESS) {
            flog::error("Could not close HackRF {0}: {1}", _this->selectedSerial, hackrf_error_name(err));
        }

        _this->stream.clearWriteStop();
        _this->rx_running = false;

        flog::info("HackRFSourceModule '{0}': Stopped Rx Mode!", _this->name);
    }

    static void start_ptt(void* ctx) {
        HackRFSourceModule* _this = (HackRFSourceModule*)ctx;
        if (_this->tx_running) { return; }

        if (_this->selectedSerial == "") {
            flog::error("Tried to start HackRF source with empty serial");
            return;
        }

        hackrf_error err = (hackrf_error)hackrf_open_by_serial(_this->selectedSerial.c_str(), &_this->openDev);
        if (err != HACKRF_SUCCESS) {
            flog::error("Could not open HackRF {0}: {1}", _this->selectedSerial, hackrf_error_name(err));
            return;
        }

        _this->current_tx_sample = 0;
        _this->biasT = true;
        _this->amp = true;
        _this->startRecording();

        hackrf_set_sample_rate(_this->openDev, _this->sampleRate);
        hackrf_set_baseband_filter_bandwidth(_this->openDev, _this->bandwidthIdToBw(_this->bwId));
        hackrf_set_freq(_this->openDev, _this->freq);
        hackrf_set_antenna_enable(_this->openDev, _this->biasT);
        hackrf_set_amp_enable(_this->openDev, _this->amp);
        hackrf_set_lna_gain(_this->openDev, _this->lna);
        hackrf_set_vga_gain(_this->openDev, _this->vga);
        hackrf_set_txvga_gain(_this->openDev, _this->tx_vga);
        hackrf_set_baseband_filter_bandwidth(_this->openDev,  _this->sampleRate*0.75);
        hackrf_start_tx(_this->openDev, callback_tx, _this);
        _this->tx_running = true;
        flog::info("HackRFSourceModule '{0} {1}': Start Tx Mode!", _this->name, _this->ptt);
    }

    static void stop_ptt(void* ctx) {
        HackRFSourceModule* _this = (HackRFSourceModule*)ctx;        
        if (!_this->tx_running) { return; }

        hackrf_error err;
        _this->stream.stopWriter();

        hackrf_stop_tx(_this->openDev);
        _this->stopRecording();

        err = (hackrf_error)hackrf_reset(_this->openDev);
        if (err != HACKRF_SUCCESS) {
            flog::error("Could not reset HackRF {0}: {1}", _this->selectedSerial, hackrf_error_name(err));
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
        flog::info("HackRF {0} has been hard reset", _this->selectedSerial);

        err = (hackrf_error)hackrf_close(_this->openDev);
        if (err != HACKRF_SUCCESS) {
            flog::error("Could not close HackRF {0}: {1}", _this->selectedSerial, hackrf_error_name(err));
        }

        _this->stream.clearWriteStop();
        _this->tx_running = false;

        flog::info("HackRFSourceModule '{0}': Stop Tx Mode!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        HackRFSourceModule* _this = (HackRFSourceModule*)ctx;
        if (_this->rx_running || _this->tx_running) {
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

        if (_this->rx_running || _this->tx_running) { SmGui::BeginDisabled(); }
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

        if (_this->rx_running || _this->tx_running) { SmGui::EndDisabled(); }

        SmGui::LeftLabel("Bandwidth");
        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("##_hackrf_bw_sel_", _this->name), &_this->bwId, bandwidthsTxt)) {
            if (_this->rx_running || _this->tx_running) {
                hackrf_set_baseband_filter_bandwidth(_this->openDev, _this->bandwidthIdToBw(_this->bwId));
            }
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["bandwidth"] = _this->bwId;
            config.release(true);
        }

        SmGui::LeftLabel("LNA Gain");
        SmGui::FillWidth();
        if (SmGui::SliderFloatWithSteps(CONCAT("##_hackrf_lna_", _this->name), &_this->lna, 0, 40, 1, SmGui::FMT_STR_FLOAT_DB_NO_DECIMAL)) {
            if (_this->rx_running) {
                hackrf_set_lna_gain(_this->openDev, _this->lna);
            }
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["lnaGain"] = (int)_this->lna;
            config.release(true);
        }

        SmGui::LeftLabel("VGA Gain");
        SmGui::FillWidth();
        if (SmGui::SliderFloatWithSteps(CONCAT("##_hackrf_vga_", _this->name), &_this->vga, 0, 62, 1, SmGui::FMT_STR_FLOAT_DB_NO_DECIMAL)) {
            if (_this->rx_running) {
                hackrf_set_vga_gain(_this->openDev, _this->vga);
            }
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["vgaGain"] = (int)_this->vga;
            config.release(true);
        }

        if (SmGui::Checkbox(CONCAT("Bias-T##_hackrf_bt_", _this->name), &_this->biasT)) {
            if (_this->rx_running || _this->tx_running) {
                hackrf_set_antenna_enable(_this->openDev, _this->biasT);
            }
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["biasT"] = _this->biasT;
            config.release(true);
        }

        if (SmGui::Checkbox(CONCAT("Amp Enabled##_hackrf_amp_", _this->name), &_this->amp)) {
            if (_this->rx_running || _this->tx_running) {
                hackrf_set_amp_enable(_this->openDev, _this->amp);
            }
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["amp"] = _this->amp;
            config.release(true);
        }

        if (SmGui::Checkbox(CONCAT("Ptt Enabled##_hackrf_ptt_", _this->name), &_this->ptt)) {            
            //_this->start_ptt(ctx);
            flog::info("HackRFSourceModule '{0}': Ptt Enabled! Value: {1}", _this->name, _this->ptt);
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["ptt"] = _this->ptt;
            config.release(true);
        }

        SmGui::LeftLabel("Tx VGA Gain");
        SmGui::FillWidth();
        if (SmGui::SliderFloatWithSteps(CONCAT("##_hackrf_tx_vga_", _this->name), &_this->tx_vga, 0, 47, 1, SmGui::FMT_STR_FLOAT_DB_NO_DECIMAL)) {
            if (_this->tx_running) {
                hackrf_set_txvga_gain(_this->openDev, _this->tx_vga);
            }
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["txVgaGain"] = (int)_this->tx_vga;
            config.release(true);
        }

        SmGui::LeftLabel("Mic Gain");
        SmGui::FillWidth();
        if (SmGui::SliderFloatWithSteps(CONCAT("##_hackrf_tx_mic_gain_", _this->name), &_this->amplitude, 0.1, 10.0, 0.1, SmGui::FMT_STR_FLOAT_DB_ONE_DECIMAL)) {
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["amplitude"] = (float)_this->amplitude;
            config.release(true);
        }

        SmGui::LeftLabel("Sensitivity");
        SmGui::FillWidth();
        if (SmGui::SliderFloatWithSteps(CONCAT("##_hackrf_tx_sensitivity_", _this->name), &_this->modulation_index, 0.1, 10.0, 0.1, SmGui:: FMT_STR_FLOAT_DB_ONE_DECIMAL)) {
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["modulation_index"] = (float)_this->modulation_index;
            config.release(true);
        }

        SmGui::LeftLabel("Filter Size");
        SmGui::FillWidth();
        if (SmGui::SliderFloatWithSteps(CONCAT("##_hackrf_tx_filter_size_", _this->name), &_this->filter_size, 0.0, 2.0, 0.2, SmGui:: FMT_STR_FLOAT_DB_ONE_DECIMAL)) {
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["filter_size"] = (float)_this->filter_size;
            config.release(true);
        }

        SmGui::LeftLabel("Interpolation");
        SmGui::FillWidth();
        if (SmGui::SliderFloatWithSteps(CONCAT("##_hackrf_interpolation_", _this->name), &_this->interpolation, 0, 200, 1, SmGui::FMT_STR_FLOAT_DB_NO_DECIMAL)) {
            config.acquire();
            config.conf["devices"][_this->selectedSerial]["interpolation"] = (float)_this->interpolation;
            config.release(true);
        }      
    }       

    static int callback_rx(hackrf_transfer* transfer) {
        HackRFSourceModule* _this = (HackRFSourceModule*)transfer->rx_ctx;
        if (!_this->rx_running) { return 0; }
        volk_8i_s32f_convert_32f((float*)_this->stream.writeBuf, (int8_t*)transfer->buffer, 128.0f, transfer->valid_length);
        if (!_this->stream.swap(transfer->valid_length / 2)) { return -1; }
        return 0;
    }

    std::vector<float> convertToFloatVector(const std::vector<dsp::complex_t>& complexBuf) {
        std::vector<float> floatBuf(2 * complexBuf.size()); // Allocate vector with double size
        for (size_t i = 0; i < complexBuf.size(); ++i) {
            floatBuf[2 * i]     = complexBuf[i].re;      // Real part
            floatBuf[2 * i + 1] = complexBuf[i].im;      // Imaginary part
        }
        return floatBuf;
    }

    std::vector<float> readStreamToSize(size_t size, std::chrono::milliseconds timeout) {
        std::vector<float> float_buffer;
        float_buffer.reserve(size);
        auto start_time = std::chrono::steady_clock::now();

        while (float_buffer.size() < size) {
            int len = stream.read();
            if (len < 0) {
                break;
            }
            std::vector<float> temp_buffer = stream.readBufferToVector();
            size_t elements_needed = size - float_buffer.size();
            size_t elements_to_add = (elements_needed < temp_buffer.size()) ? elements_needed : temp_buffer.size();
            float_buffer.insert(float_buffer.end(), temp_buffer.begin(), temp_buffer.begin() + elements_to_add);
            stream.flush();
            if (std::chrono::steady_clock::now() - start_time > timeout) {
                std::cerr << "Timeout reached while reading stream" << std::endl;
                break;
            }
        }
        return float_buffer;
    }

    int apply_modulation(int8_t* buffer, uint32_t length) {

        int decimation = 1;
        size_t desired_size = length / sizeof(float);
        auto timeout_ms = std::chrono::milliseconds(static_cast<long long>(1000.0 * desired_size / AUDIO_SAMPLE_RATE));
        timeout_ms += std::chrono::milliseconds(50);
        std::vector<float> float_buffer = readStreamToSize(desired_size, timeout_ms);

        if (float_buffer.size() < desired_size) {
            return 0;
        }

        int noutput_items = float_buffer.size();
        for (int i = 0; i < noutput_items; ++i) {
            float_buffer[i] *= this->amplitude;
        }

        std::cout << float_buffer.size() << std::endl;

        std::vector<std::complex<float>> modulated_signal(noutput_items);
        float sensitivity = modulation_index;
        FrequencyModulator modulator(sensitivity);
        modulator.work(noutput_items, float_buffer, modulated_signal);

        RationalResampler resampler(interpolation, decimation, filter_size);
        std::vector<std::complex<float>> resampled_signal = resampler.resample(modulated_signal);

        for (int i = 0; i < noutput_items; ++i) {
            buffer[2 * i] = static_cast<int8_t>(std::real(resampled_signal[i]) * 127.0f);
            buffer[2 * i + 1] = static_cast<int8_t>(std::imag(resampled_signal[i]) * 127.0f);
        }

        return 0;
    }

    int apply_wfm_modulation(int8_t *buffer, size_t length) {
        static double phase = 0.0;
        const double modulation_sensitivity = 2.0 * M_PI * FREQUENCY_DEVIATION / sampleRate;
        const double sine_wave_increment = 2.0 * M_PI * SINE_WAVE_FREQUENCY / sampleRate;

        for (size_t i = 0; i < length; ++i) {
            double sine_wave_sample = sin(phase) * this->amplitude;
            phase += modulation_sensitivity * sine_wave_sample;
            while (phase >= 2.0 * M_PI) {
                phase -= 2.0 * M_PI;
            }
            while (phase < 0) {
                phase += 2.0 * M_PI;
            }
            double sample = 127.0 * sin(phase);
            if (sample > 127.0) sample = 127.0;
            else if (sample < -127.0) sample = -127.0;
            buffer[i] = static_cast<int8_t>(sample);
            phase += sine_wave_increment;
            if (phase >= 2.0 * M_PI) {
                phase -= 2.0 * M_PI;
            }
        }
        return 0;
    }

    int apply_am_modulation(int8_t *buffer, size_t length) {
        static double carrier_phase = 0.0;
        static double mod_phase = 0.0;
        const double carrier_phase_increment = 2.0 * M_PI * freq / sampleRate;
        const double mod_phase_increment = 2.0 * M_PI * SINE_WAVE_FREQUENCY / sampleRate;

        for (size_t i = 0; i < length; ++i) {
            double carrier = sin(carrier_phase);
            double modulator = 1.0 + AM_MOD_INDEX * sin(mod_phase);
            double sample = 127.0 * carrier * modulator;
            if (sample > 127.0) sample = 127.0;
            else if (sample < -127.0) sample = -127.0;
            buffer[i] = static_cast<int8_t>(sample);
            carrier_phase += carrier_phase_increment;
            if (carrier_phase >= 2.0 * M_PI) {
                carrier_phase -= 2.0 * M_PI;
            }
            mod_phase += mod_phase_increment;
            if (mod_phase >= 2.0 * M_PI) {
                mod_phase -= 2.0 * M_PI;
            }
        }
        return 0;
    }

    static int callback_tx(hackrf_transfer* transfer) {
        HackRFSourceModule* _this = (HackRFSourceModule*)transfer->tx_ctx;
        if (!_this->tx_running) { return 0; }

        return _this->apply_modulation((int8_t *)transfer->buffer, transfer->valid_length);
        //return _this->apply_wfm_modulation((int8_t *)transfer->buffer, transfer->valid_length);
    }        

    std::string name;
    hackrf_device* openDev;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    int sampleRate;
    SourceManager::SourceHandler handler;
    bool rx_running = false;
    bool tx_running = false;
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
    float amplitude = 1.0;
    float filter_size = 0;
    float modulation_index = 0;
    float interpolation = 0;
    int current_tx_sample = 0;

    RtAudiSource *rtAudioSource;

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
