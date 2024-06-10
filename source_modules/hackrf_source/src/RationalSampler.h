#ifndef RATIONALSAMPLER_H
#define RATIONALSAMPLER_H
#include <iostream>
#include <vector>
#include <cmath>
#include <complex>
#include <algorithm>
#include <numeric>
#include <stdexcept>

class RationalSampler {
public:
    RationalSampler(unsigned interpolation, unsigned decimation, float fractional_bw)
        : interpolation(interpolation), decimation(decimation), fractional_bw(fractional_bw) {
        if (fractional_bw >= 0.5 || fractional_bw <= 0) {
            throw std::range_error("Invalid fractional_bandwidth, must be in (0, 0.5)");
        }
        if (interpolation == 0 || decimation == 0) {
            throw std::out_of_range("Interpolation and decimation factors must be > 0");
        }

        // Reduce interpolation and decimation by their greatest common divisor
        auto gcd = std::gcd(interpolation, decimation);
        this->interpolation /= gcd;
        this->decimation /= gcd;

        // Design filter taps
        taps = design_resampler_filter(this->interpolation, this->decimation, this->fractional_bw);

        // Create polyphase filters
        int num_taps = taps.size();
        int filter_len = num_taps / this->interpolation;
        polyphase_filters.resize(this->interpolation);
        for (int i = 0; i < this->interpolation; i++) {
            polyphase_filters[i].resize(filter_len);
            for (int j = 0; j < filter_len; j++) {
                polyphase_filters[i][j] = taps[i + j * this->interpolation];
            }
        }
    }

    std::vector<std::complex<float>> resample(const std::vector<std::complex<float>>& input) {
        std::vector<std::complex<float>> output((input.size() * interpolation) / decimation + 1);
        unsigned ctr = 0;
        int out_idx = 0;

        for (size_t in_idx = 0; in_idx < input.size(); ++in_idx) {
            for (int j = 0; j < interpolation; ++j) {
                std::complex<float> sum = 0;
                for (int k = 0; k < polyphase_filters[j].size(); ++k) {
                    if (in_idx >= k) {
                        sum += input[in_idx - k] * polyphase_filters[j][k];
                    }
                }
                if (ctr == 0) {
                    output[out_idx++] = sum;
                }
                ctr = (ctr + decimation) % interpolation;
            }
        }

        output.resize(out_idx);
        return output;
    }

private:
    std::vector<float> design_resampler_filter(unsigned interpolation, unsigned decimation, float fractional_bw) {
        float beta = 7.0;
        float halfband = 0.5;
        float rate = float(interpolation) / float(decimation);
        float trans_width, mid_transition_band;

        if (rate >= 1.0) {
            trans_width = halfband - fractional_bw;
            mid_transition_band = halfband - trans_width / 2.0;
        } else {
            trans_width = rate * (halfband - fractional_bw);
            mid_transition_band = rate * halfband - trans_width / 2.0;
        }

        int num_taps = static_cast<int>(4 / trans_width);
        std::vector<float> taps(num_taps);
        float sum = 0.0f;
        for (int i = 0; i < num_taps; i++) {
            float x = 2 * M_PI * (i - (num_taps - 1) / 2.0f);
            float sinc = (i == (num_taps - 1) / 2) ? 1.0f : sin(mid_transition_band * x) / (mid_transition_band * x);
            float window = 0.54 - 0.46 * cos(2 * M_PI * i / (num_taps - 1));
            taps[i] = sinc * window;
            sum += taps[i];
        }

        for (float& tap : taps) {
            tap /= sum;
        }

        return taps;
    }

    unsigned interpolation;
    unsigned decimation;
    float fractional_bw;
    std::vector<float> taps;
    std::vector<std::vector<float>> polyphase_filters;
};

#endif // RATIONALSAMPLER_H
