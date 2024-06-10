#ifndef FREQUENCYMODULATOR_H
#define FREQUENCYMODULATOR_H
#include <iostream>
#include <vector>
#include <cmath>
#include <complex>

#define F_PI ((float)(M_PI))

namespace fxpt {
constexpr int32_t FIXED_POINT_ONE = 1 << 16;

int32_t float_to_fixed(float x) {
    return static_cast<int32_t>(x * FIXED_POINT_ONE);
}

void sincos(int32_t angle, float* sin_out, float* cos_out) {
    float radians = static_cast<float>(angle) / FIXED_POINT_ONE;
    *sin_out = std::sin(radians);
    *cos_out = std::cos(radians);
}
}

class FrequencyModulator {
public:
    FrequencyModulator(float sensitivity)
        : d_sensitivity(sensitivity), d_phase(0.0f) {}

    int work(int noutput_items, const std::vector<float>& input_items, std::vector<std::complex<float>>& output_items) {
        for (int i = 0; i < noutput_items; ++i) {
            d_phase += d_sensitivity * input_items[i];

            // Place phase in [-pi, +pi[
            d_phase = std::fmod(d_phase + F_PI, 2.0f * F_PI) - F_PI;

            float oi, oq;
            int32_t angle = fxpt::float_to_fixed(d_phase);
            fxpt::sincos(angle, &oq, &oi);
            output_items[i] = std::complex<float>(oi, oq);
        }
        return noutput_items;
    }

private:
    float d_sensitivity;
    float d_phase;
};

#endif // FREQUENCYMODULATOR_H
