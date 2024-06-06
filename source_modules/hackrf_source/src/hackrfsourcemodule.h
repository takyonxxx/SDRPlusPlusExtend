#ifndef HACKRFSOURCEMODULE_H
#define HACKRFSOURCEMODULE_H

#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include <condition_variable>
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
#include "circular_buffer.h"
#include "constants.h"
#include "micreader.h"

#ifndef __ANDROID__
#include <libhackrf/hackrf.h>
#else
#include <android_backend.h>
#include <hackrf.h>
#endif


namespace fs = std::filesystem;

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

class LowPassFilter {
private:
    double alpha;
    double y_prev;

public:
    LowPassFilter(double sampleRate, double cutoffFreq) {
        double dt = 1.0 / sampleRate;
        double RC = 1.0 / (2 * M_PI * cutoffFreq);
        alpha = dt / (RC + dt);
        y_prev = 0.0;
    }

    double filter(double x) {
        double y = alpha * x + (1 - alpha) * y_prev;
        y_prev = y;
        return y;
    }

};

#endif // HACKRFSOURCEMODULE_H

