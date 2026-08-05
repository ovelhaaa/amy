#include "test_tone.h"
#include <cmath>

namespace smk {


#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif



TestTone::TestTone(float freq_hz, float amplitude_db) 
    : _mode(Mode::SILENCE), _freq_hz(freq_hz), _phase(0.0f) {
    _amplitude_lin = std::pow(10.0f, amplitude_db / 20.0f) * 32767.0f;
}

void TestTone::setMode(Mode mode) {
    _mode = mode;
}

void TestTone::generate(int16_t* buffer, size_t frames, uint32_t sample_rate_hz) {
    if (_mode == Mode::SILENCE) {
        for (size_t i = 0; i < frames * 2; ++i) {
            buffer[i] = 0;
        }
        return;
    }

    float phase_inc = 2.0f * (float)M_PI * _freq_hz / sample_rate_hz;

    for (size_t i = 0; i < frames; ++i) {
        int16_t val = (int16_t)(std::sin(_phase) * _amplitude_lin);
        
        if (_mode == Mode::LEFT_ONLY) {
            buffer[i * 2] = val;
            buffer[i * 2 + 1] = 0;
        } else if (_mode == Mode::RIGHT_ONLY) {
            buffer[i * 2] = 0;
            buffer[i * 2 + 1] = val;
        } else { // STEREO or SWEEP (Sweep logic not fully impl here)
            buffer[i * 2] = val;
            buffer[i * 2 + 1] = val;
        }
        
        _phase += phase_inc;
        if (_phase >= 2.0f * (float)M_PI) {
            _phase -= 2.0f * (float)M_PI;
        }
    }
}


} // namespace smk
