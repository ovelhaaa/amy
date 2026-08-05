#pragma once
#include <cstdint>
#include <cstddef>

namespace smk {




class TestTone {
public:
    enum class Mode {
        SILENCE,
        LEFT_ONLY,
        RIGHT_ONLY,
        STEREO,
        SWEEP
    };

    TestTone(float freq_hz = 440.0f, float amplitude_db = -6.0f);
    void setMode(Mode mode);
    void generate(int16_t* buffer, size_t frames, uint32_t sample_rate_hz);

private:
    Mode _mode;
    float _freq_hz;
    float _amplitude_lin;
    float _phase;
};


} // namespace smk
