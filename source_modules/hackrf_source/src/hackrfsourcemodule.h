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

std::vector<float> generate_lowpass_fir_coefficients(float cutoff_freq, int sample_rate, int num_taps) {
    std::vector<float> coefficients(num_taps);
    float norm_cutoff = cutoff_freq / (sample_rate / 2.0); // Normalize cutoff frequency
    int M = num_taps - 1;
    for (int i = 0; i < num_taps; ++i) {
        if (i == M / 2) {
            coefficients[i] = norm_cutoff;
        } else {
            coefficients[i] = norm_cutoff * (std::sin(M_PI * norm_cutoff * (i - M / 2)) / (M_PI * norm_cutoff * (i - M / 2)));
        }
        coefficients[i] *= 0.54 - 0.46 * std::cos(2 * M_PI * i / M); // Hamming window
    }
    return coefficients;
}

std::vector<float> apply_fir_filter(const std::vector<float>& input, const std::vector<float>& coefficients) {
    int input_size = input.size();
    int num_taps = coefficients.size();
    std::vector<float> output(input_size, 0.0f);

    for (int i = num_taps / 2; i < input_size - num_taps / 2; ++i) {
        for (int j = 0; j < num_taps; ++j) {
            output[i] += input[i - num_taps / 2 + j] * coefficients[j];
        }
    }

    return output;
}

std::vector<float> rational_resampler(const std::vector<float>& input, int interpolation, int decimation) {
    // Calculate output size
    size_t output_size = input.size() * interpolation / decimation;
    std::vector<float> output(output_size);

    // Resampling loop
    for (size_t i = 0; i < output_size; ++i) {
        float input_index = i * decimation / interpolation;
        int lower_index = static_cast<int>(std::floor(input_index));
        int upper_index = static_cast<int>(std::ceil(input_index));

        // Check boundary conditions
        if (lower_index < 0 || upper_index >= static_cast<int>(input.size())) {
            output[i] = 0.0f; // Zero-padding for out-of-bounds access
        } else {
            // Linear interpolation
            float t = input_index - lower_index;
            output[i] = (1 - t) * input[lower_index] + t * input[upper_index];
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

std::vector<float> frequency_modulator(const std::vector<float>& input, float sensitivity, float sample_rate, float carrier_freq) {
    std::vector<float> output;
    output.reserve(input.size());

    float phase = 0.0f;
    float phase_increment = 2 * M_PI * carrier_freq / sample_rate;

    for (auto sample : input) {
        phase += phase_increment + sensitivity * sample;
        output.push_back(std::sin(phase));
    }

    return output;
}

#endif // HACKRFSOURCEMODULE_H

