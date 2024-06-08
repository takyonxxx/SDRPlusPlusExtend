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

std::vector<float> low_pass_filter(const std::vector<float>& input, float cutoff_freq, float sample_rate) {
    const int filter_order = 101; // Define filter order
    std::vector<float> filter_coeffs(filter_order);

    float norm_cutoff = cutoff_freq / (sample_rate / 2.0);
    for (int i = 0; i < filter_order; ++i) {
        if (i == (filter_order - 1) / 2) {
            filter_coeffs[i] = norm_cutoff;
        } else {
            float arg = M_PI * norm_cutoff * (i - (filter_order - 1) / 2);
            filter_coeffs[i] = norm_cutoff * (std::sin(arg) / arg);
        }
        filter_coeffs[i] *= (0.54 - 0.46 * std::cos(2 * M_PI * i / (filter_order - 1))); // Hamming window
    }

    std::vector<float> output(input.size(), 0); // Output size is same as input

    // Convolution with filter coefficients
    for (size_t i = 0; i < input.size(); ++i) {
        for (int j = 0; j < filter_order; ++j) {
            if (i + j < input.size()) {
                output[i] += input[i + j] * filter_coeffs[j];
            }
        }
    }

    return output;
}

std::vector<float> multiply_const(const std::vector<float>& input, float constant) {
    std::vector<float> output;
    output.reserve(input.size());

    for (auto sample : input) {
        output.push_back(sample * constant);
    }

    return output;
}

std::vector<float> rational_resampler(const std::vector<float>& input, double interpolation, int decimation) {
    size_t output_size = static_cast<size_t>(input.size() * interpolation / decimation);
    std::vector<float> output(output_size);

    for (size_t i = 0; i < output_size; ++i) {
        double input_index = i * decimation / interpolation;
        int lower_index = static_cast<int>(input_index);
        int upper_index = lower_index + 1;

        // Check boundary conditions
        if (lower_index < 0 || upper_index >= static_cast<int>(input.size())) {
            output[i] = 0.0f; // Zero-padding for out-of-bounds access
        } else {
            double t = input_index - lower_index;
            output[i] = (1.0 - t) * input[lower_index] + t * input[upper_index];
        }
    }

    return output;
}

std::vector<float> frequency_modulator(const std::vector<float>& input, float sensitivity) {
    std::vector<float> output;
    output.reserve(input.size());

    float phase = 0.0f;
    for (auto sample : input) {
        phase += sensitivity * sample;
        output.push_back(std::sin(phase));
    }
    return output;
}

#endif // HACKRFSOURCEMODULE_H

