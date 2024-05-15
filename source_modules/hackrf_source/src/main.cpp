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

#include "wavreader.h"
#include "demod.h"
#include "recorder.h"

namespace fs = std::filesystem;

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
const char* sampleRatesTxt = "20MHz\00016MHz\00010MHz\0008MHz\0005MHz\0004MHz\0002MHz\0001MHz\000";

const int sampleRates[] = {
    20000000,
    16000000,
    10000000,
    8000000,
    5000000,
    4000000,
    2000000,
    1000000,
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

        paRecorder = new PaRecorder();
        paRecorder->initPa();

        hackrf_init();

        // Select the last samplerate option
        sampleRate = 1000000;
        srId = 7;

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

        // std::string marker = "CMakeLists.txt";
        // std::string projectFolder = findProjectFolder(marker);
        // std::string filePath = projectFolder + "/source_modules/hackrf_source/src/input.wav";
        // const char* path = filePath.c_str();
        // prepareWaveData(path);
    }

    ~HackRFSourceModule() {
        paRecorder->finishPa();
        delete paRecorder;
        stop(this);
        hackrf_exit();
        sigpath::sourceManager.unregisterSource("HackRF");
        delete[] _iqCache[0];
        delete[] _iqCache;
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
            config.conf["devices"][serial]["sampleRate"] = 1000000;
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
        sampleRate = 1000000;
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
            _this->paRecorder->startPaCallback();
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

        _this->running = false;
        _this->stream.stopWriter();

        if(_this->ptt)
        {            
            hackrf_stop_tx(_this->openDev);
            _this->paRecorder->stopPaCallback();
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

    int send_wav_tx(int8_t *buffer, uint32_t length) {
        if(_iqCache)
        {
            int _nsample = (float)_audioSampleRate * (float)BUF_LEN / (float)sampleRate / 2.0;
            int totalSampleCount = _numSampleCount / _nsample;

            if (_buffCount >= totalSampleCount) {
                _buffCount = 0;
            }

            if (_buffCount < totalSampleCount) {
                uint32_t copyLength = std::min(length, static_cast<uint32_t>(BUF_LEN));
                std::copy_n(_iqCache[_buffCount], copyLength, buffer);
                _buffCount++;
            }
        }
        return 0;
    }

    int send_mic_tx(int8_t *buffer, uint32_t length) {
        std::unique_lock<std::mutex> lock(s_audioBufferMutex);

        // Wait until s_audioBufferQueue is not empty
        s_audioBufferCondition.wait(lock, []{ return !s_audioBufferQueue.empty(); });

        // Once s_audioBufferQueue is not empty, proceed
        std::vector<float> audioData = std::move(s_audioBufferQueue.front());
        s_audioBufferQueue.pop();

        lock.unlock(); // Unlock before calling interpolate_and_modulate

        // Process the audio data in chunks to fill the buffer
        uint32_t remainingLength = length;
        uint32_t offset = 0;

        while (remainingLength > 0) {
            uint32_t chunkSize = std::min(remainingLength, (uint32_t)audioData.size());
            if (chunkSize > 0) {
                // Interpolate and modulate a chunk of audio data to match the length
                interpolate_and_modulate(buffer + offset, audioData, chunkSize, 0, sampleRate);
                offset += chunkSize;
                remainingLength -= chunkSize;
            } else {
                // If the audio data is smaller than the buffer, handle the mismatch
                std::cerr << "Error: Buffer length exceeds available audio data size." << std::endl;
                break;
            }
        }

        std::cout << "Buffer size: " << length << std::endl;

        return 0;
    }

    static int callback_tx(hackrf_transfer* transfer) {
        HackRFSourceModule* _this = (HackRFSourceModule*)transfer->tx_ctx;
        if(_this->_iqCache)
        {
            return _this->send_wav_tx((int8_t *)transfer->buffer, transfer->valid_length);
        }
        else
        {
            return _this->send_mic_tx((int8_t *)transfer->buffer, transfer->valid_length);
        }
        return 0;
    }  

    void prepareWaveData(const char *path){

        WaveData *wave = wavRead(path, strlen(path));
        int nch = wave->header.numChannels;
        _audioSampleRate = wave->sampleRate;
        _numSampleCount = wave->size / wave->header.blockAlign;

        std::cout<< _numSampleCount << std::endl;

        _audioSampleBuf=new float[_numSampleCount]();
        _new_audio_buf = new float[BUF_LEN/2]();
        _new_audio_buf1 = new float[BUF_LEN/2]();
        _new_audio_buf2 = new float[BUF_LEN/2]();
        _new_audio_buf3 = new float[BUF_LEN/2]();

        if(nch==1){

            for(int i=0;i<_numSampleCount;i++){

                _audioSampleBuf[i] = wave->samples[i];
            }

        }else if(nch==2){

            for(int i=0;i<_numSampleCount;i++){

                _audioSampleBuf[i] = (wave->samples[i * 2] + wave->samples[i * 2 + 1]) / (float)2.0;
            }
        }

        int _nsample = (float) _audioSampleRate * (float) BUF_LEN / (float) sampleRate / 2.0;

        _iqCache = new int8_t *[_numSampleCount / _nsample]();
        for (int i = 0; i < _numSampleCount / _nsample; i++) {
            _iqCache[i] = new int8_t[BUF_LEN]();
        }

#pragma omp parallel for
        for (int i = 0; i < _numSampleCount / _nsample; i++) {
            if (i < _numSampleCount / _nsample / 4) {
                interpolation(_audioSampleBuf + (_nsample * i), _nsample, _new_audio_buf, BUF_LEN / 2);
                modulation(_new_audio_buf, _iqCache[i], 0, sampleRate);
            }
            else if (i < _numSampleCount / _nsample / 4 * 2) {
                interpolation(_audioSampleBuf + (_nsample * i), _nsample, _new_audio_buf1, BUF_LEN / 2);
                modulation(_new_audio_buf1, _iqCache[i], 0, sampleRate);
            }
            else if (i < _numSampleCount / _nsample / 4 * 3) {
                interpolation(_audioSampleBuf + (_nsample * i), _nsample, _new_audio_buf2, BUF_LEN / 2);
                modulation(_new_audio_buf2, _iqCache[i], 0, sampleRate);
            }
            else if (i < _numSampleCount / _nsample) {
                interpolation(_audioSampleBuf + (_nsample * i), _nsample, _new_audio_buf3, BUF_LEN / 2);
                modulation(_new_audio_buf3, _iqCache[i], 0, sampleRate);
            }
        }
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

    // for wav file
    int _audioSampleRate=0;
    float * _audioSampleBuf=NULL;
    float * _new_audio_buf=NULL;
    float * _new_audio_buf1=NULL;
    float * _new_audio_buf2=NULL;
    float * _new_audio_buf3=NULL;
    unsigned int offset=0;
    int32_t  _numSampleCount;
    int8_t** _iqCache = nullptr;
    int _buffCount = 0;

    std::mutex bufferMutex;
    std::vector<float> audioBuffer;
    PaRecorder *paRecorder = nullptr;

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
