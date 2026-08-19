#pragma once
#include "synth_engine.h"

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
    void loadPreset(uint8_t osc_id, uint16_t preset_id);
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
};

} // namespace smk
