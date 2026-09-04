#pragma once
#include "synth_engine.h"
#include <atomic>

namespace smk {

class AmyAdapter : public SynthEngine {
public:
    AmyAdapter();
    virtual ~AmyAdapter();

    bool begin(uint32_t sample_rate_hz) override;
    void noteOn(uint8_t channel, uint8_t note, uint8_t velocity) override;
    void noteOff(uint8_t channel, uint8_t note) override;
    void pitchBend(uint8_t channel, int16_t value) override;
    void controlChange(uint8_t channel, uint8_t controller, uint8_t value) override;
    void allNotesOff() override;
    void panic() override;
    int16_t* render() override;
    uint16_t blockSize() const override;
    float renderLoad() const override;
    uint32_t activeVoices() const override;

    // Direct AMY sound parameter controls
    void setFilter(uint8_t osc_id, float cutoff_hz, float resonance);
    void setOscillatorWaveform(uint8_t osc_id, uint8_t wave_type);
    void setEnvelope(uint8_t osc_id, float attack_ms, float decay_ms, float sustain_level, float release_ms);
    void setPortamento(uint8_t synth_id, uint16_t portamento_ms);
    void loadPreset(uint8_t synth_id, uint16_t preset_id, uint8_t num_voices = 8);
    void sendAmyMessage(const char* message);

    // FM Synthesis Controls
    void setFmModIndex(uint8_t osc_id, float mod_index);
    void setFmFeedback(uint8_t osc_id, float feedback);
    void setFmRatio(uint8_t osc_id, float ratio);
    void setFmAlgorithm(uint8_t osc_id, uint8_t algo_id);

    // Built-in AMY Effects Controls
    void setChorus(float depth, float rate, float level);
    void setReverb(float room_size, float damp, float mix);
    void setDelay(float delay_ms, float feedback, float mix);
    void setSendLevels(uint8_t osc_id, float reverb_send, float chorus_send, float echo_send);

    // Master Output & Protection
    void setSoftLimiter(bool enable) { soft_limiter_enabled_.store(enable, std::memory_order_relaxed); }
    bool softLimiterEnabled() const { return soft_limiter_enabled_.load(std::memory_order_relaxed); }
    void setMasterGain(float gain);
    float masterGain() const { return master_gain_.load(std::memory_order_relaxed); }

    // Oscilloscope sample capture for UI
    static constexpr size_t kScopeBufferSize = 128;
    static constexpr size_t kRawScopeBufferSize = 512; // stereo samples (256 frames * 2)
    void getScopeSamples(int16_t* dest, size_t max_count, size_t* out_count = nullptr) const override;

    // Monophonic Legato
    void setMonoMode(bool enable) { mono_mode_ = enable; mono_stack_size_ = 0; }
    bool isMonoMode() const { return mono_mode_; }

private:
    std::atomic<uint32_t> active_voices_{0};
    std::atomic<bool> soft_limiter_enabled_{true};
    std::atomic<float> master_gain_{1.0f};
    uint8_t active_notes_[16][128]{};
    int16_t scope_buffer_[kRawScopeBufferSize]{};

    static constexpr size_t kMonoStackCap = 16;
    uint8_t mono_stack_[kMonoStackCap]{};
    uint8_t mono_stack_size_{0};
    bool mono_mode_{false};
};

} // namespace smk
