#ifndef RATIONALRESAMPLER_H
#define RATIONALRESAMPLER_H
#include <iostream>
#include <vector>
#include <cmath>
#include <complex>
#include <stdexcept>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <boost/math/special_functions/bessel.hpp>

namespace firdes {
const double GR_M_PI = 3.14159265358979323846;

enum Window {
    WIN_KAISER
};

void sanity_check_1f(double sampling_freq, double cutoff_freq, double transition_width) {
    if (sampling_freq <= 0 || cutoff_freq <= 0 || transition_width <= 0 || cutoff_freq + transition_width / 2 >= sampling_freq / 2) {
        throw std::invalid_argument("Invalid frequency parameters");
    }
}

int compute_ntaps(double sampling_freq, double transition_width, Window window_type, double param) {
    return static_cast<int>(4 / (transition_width / sampling_freq)); // Example computation
}

std::vector<float> window(Window window_type, int ntaps, double param) {
    std::vector<float> w(ntaps, 1.0); // Rectangular window as an example

    if (window_type == WIN_KAISER) {
        for (int i = 0; i < ntaps; ++i) {
            double alpha = (ntaps - 1) / 2.0;
            double ratio = (i - alpha) / alpha;
            w[i] = boost::math::cyl_bessel_i(0, param * std::sqrt(1 - ratio * ratio)) / boost::math::cyl_bessel_i(0, param);
        }
    }
    return w;
}

std::vector<float> low_pass(double gain, double sampling_freq, double cutoff_freq, double transition_width, Window window_type, double param) {
    sanity_check_1f(sampling_freq, cutoff_freq, transition_width);

    int ntaps = compute_ntaps(sampling_freq, transition_width, window_type, param);
    std::vector<float> taps(ntaps);
    std::vector<float> w = window(window_type, ntaps, param);

    int M = (ntaps - 1) / 2;
    double fwT0 = 2 * GR_M_PI * cutoff_freq / sampling_freq;

    for (int n = -M; n <= M; n++) {
        if (n == 0) {
            taps[n + M] = fwT0 / GR_M_PI * w[n + M];
        } else {
            taps[n + M] = sin(n * fwT0) / (n * GR_M_PI) * w[n + M];
        }
    }

    double fmax = taps[0 + M];
    for (int n = 1; n <= M; n++) {
        fmax += 2 * taps[n + M];
    }

    gain /= fmax;

    for (int i = 0; i < ntaps; i++) {
        taps[i] *= gain;
    }

    return taps;
}
}

template <typename TAP_T>
std::vector<TAP_T> design_resampler_filter(const unsigned interpolation, const unsigned decimation, const float fractional_bw) {
    if (fractional_bw >= 0.5 || fractional_bw <= 0) {
        throw std::range_error("Invalid fractional_bandwidth " + std::to_string(fractional_bw) + ", must be in (0, 0.5)");
    }

    float beta = 7.0;
    float halfband = 0.5;
    float rate = static_cast<float>(interpolation) / static_cast<float>(decimation);
    float trans_width, mid_transition_band;

    if (rate >= 1.0) {
        trans_width = halfband - fractional_bw;
        mid_transition_band = halfband - trans_width / 2.0;
    } else {
        trans_width = rate * (halfband - fractional_bw);
        mid_transition_band = rate * halfband - trans_width / 2.0;
    }

    return firdes::low_pass(static_cast<double>(interpolation),       // gain
                            static_cast<double>(interpolation),       // Fs
                            static_cast<double>(mid_transition_band), // trans mid point
                            static_cast<double>(trans_width),         // transition width
                            firdes::WIN_KAISER,                       // window type
                            static_cast<double>(beta));               // beta
}

template <typename TAP_T>
std::vector<std::complex<TAP_T>> resample(const std::vector<std::complex<TAP_T>>& input_signal, const unsigned interpolation, const unsigned decimation, const std::vector<TAP_T>& filter_taps) {
    std::vector<std::complex<TAP_T>> output_signal;
    int input_size = input_signal.size();
    int filter_size = filter_taps.size();
    int M = (filter_size - 1) / 2;
    int output_size = (input_size * interpolation) / decimation;

    output_signal.reserve(output_size);

    for (int i = 0; i < output_size; ++i) {
        std::complex<TAP_T> output_sample = 0;
        int index = (i * decimation) / interpolation;

        for (int j = -M; j <= M; ++j) {
            int tap_index = M + j;
            int sample_index = index + j;
            if (sample_index >= 0 && sample_index < input_size) {
                output_sample += input_signal[sample_index] * filter_taps[tap_index];
            }
        }

        output_signal.push_back(output_sample);
    }

    return output_signal;
}

template <typename TAP_T>
std::vector<std::complex<TAP_T>> design_and_resample(const std::vector<std::complex<TAP_T>>& input_signal, const unsigned interpolation, const unsigned decimation, const float fractional_bw) {
    auto filter_taps = design_resampler_filter<TAP_T>(interpolation, decimation, fractional_bw);
    return resample(input_signal, interpolation, decimation, filter_taps);
}



//class RationalResampler {
//public:
//    RationalResampler(unsigned interpolation, unsigned decimation, float filter_size)
//        : interpolation(interpolation), decimation(decimation), filter_size(filter_size) {
//        if (interpolation == 0 || decimation == 0) {
//            throw std::out_of_range("Interpolation and decimation factors must be greater than zero");
//        }
//    }

//    std::vector<std::complex<float>> resample(const std::vector<std::complex<float>>& input) {

//        std::vector<std::complex<float>> filtered_input = apply_low_pass_filter(input);

//        std::vector<std::complex<float>> interpolated_output;
//        interpolated_output.reserve(input.size() * interpolation);

//        for (size_t i = 0; i < filtered_input.size() - 1; ++i) {
//            interpolated_output.push_back(filtered_input[i]);
//            for (unsigned j = 1; j < interpolation; ++j) {
//                std::complex<float> interpolated_sample;
//                float t = static_cast<float>(j) / static_cast<float>(interpolation);
//                interpolated_sample.real((1.0f - t) * filtered_input[i].real() + t * filtered_input[i + 1].real());
//                interpolated_sample.imag((1.0f - t) * filtered_input[i].imag() + t * filtered_input[i + 1].imag());
//                interpolated_output.push_back(interpolated_sample);
//            }
//        }
//        interpolated_output.push_back(filtered_input.back());

//        std::vector<std::complex<float>> output;
//        output.reserve(interpolated_output.size() / decimation);

//        for (size_t i = 0; i < interpolated_output.size(); i += decimation) {
//            output.push_back(interpolated_output[i]);
//        }

//        return output;
//    }

//private:
//    unsigned interpolation;
//    unsigned decimation;
//    float filter_size;

//    std::vector<std::complex<float>> apply_low_pass_filter(const std::vector<std::complex<float>>& input) {
//        std::vector<float> filter;
//        int num_taps = static_cast<int>(7 * filter_size);
//        float sum = 0.0f;

//        for (int i = 0; i < num_taps; ++i) {
//            float tap_value = std::exp(-0.5f * std::pow(i - (num_taps - 1) / 2.0f, 2) / (2 * std::pow(filter_size, 2)));
//            filter.push_back(tap_value);
//            sum += tap_value;
//        }

//        // Filtre normalizasyonu
//        for (auto& tap : filter) {
//            tap /= sum;
//        }

//        std::vector<std::complex<float>> filtered_input;

//        // Filtreyi uygulama
//        for (size_t i = 0; i < input.size(); ++i) {
//            std::complex<float> sum = 0;
//            for (size_t j = 0; j < filter.size(); ++j) {
//                if (i >= j) {
//                    sum += input[i - j] * filter[j];
//                }
//            }
//            filtered_input.push_back(sum);
//        }

//        if(filter_size != 0)
//            return filtered_input;
//        else
//            return input;
//    }
//};

#endif // RATIONALRESAMPLER_H
