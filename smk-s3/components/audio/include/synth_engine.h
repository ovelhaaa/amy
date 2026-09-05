#pragma once
#include <cstdint>
#include <cstddef>

namespace smk {




class SynthEngine {
public:
    virtual ~SynthEngine() = default;
    virtual bool begin(uint32_t sample_rate_hz) = 0;
    virtual void noteOn(uint8_t channel, uint8_t note, uint8_t velocity) = 0;
    virtual void noteOff(uint8_t channel, uint8_t note) = 0;
    virtual void pitchBend(uint8_t channel, int16_t value) = 0;
    virtual void controlChange(uint8_t channel, uint8_t controller, uint8_t value) = 0;
    virtual void allNotesOff() = 0;
    virtual void panic() = 0;
    virtual int16_t* render() = 0;
    virtual uint16_t blockSize() const = 0;
    virtual float renderLoad() const = 0;
    virtual uint32_t activeVoices() const = 0;
    // Buffered engines publish synthesis metrics themselves; render() only
    // consumes prepared PCM. Stop notification must not wait for the producer.
    virtual bool bufferedOutput() const { return false; }
    virtual void onAudioStopped() {}
    virtual void getScopeSamples(int16_t* dest, size_t max_count, size_t* out_count = nullptr) const {
        (void)dest; (void)max_count; if (out_count) *out_count = 0;
    }
};


} // namespace smk
