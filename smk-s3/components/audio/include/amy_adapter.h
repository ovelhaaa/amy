#pragma once
#include "synth_engine.h"
#include <atomic>

namespace smk {

class AmyAdapter : public SynthEngine {
public:
    AmyAdapter();
    virtual ~AmyAdapter();

    bool begin(uint32_t sample_rate_hz) override;
    bool startWorker(uint8_t core_id, uint8_t priority, uint32_t stack_size_bytes);
    bool bufferedOutput() const override { return true; }
    void onAudioStopped() override;
    // Control task propagates an emergency queue reset to note generators.
    bool takeRecoveryRequest();
    // Restarts the synthesis owner's internal render-time EWMA from the next
    // rendered block. Called by audio_reset so a fresh qualification window is
    // not blended with pre-reset timing. Lock-free; safe from any task.
    void resetRenderAverage();
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

    // Copied commands; only the synthesis worker accesses AMY after boot.
    virtual void setFilter(uint8_t osc_id, float cutoff_hz, float resonance,
                           float env_amount = 0.0f, float key_tracking = 0.0f,
                           float vel_tracking = 1.5f, uint8_t filter_type = 0);
    virtual void setOscillatorWaveform(uint8_t osc_id, uint8_t wave_type);
    virtual void setEnvelope(uint8_t osc_id, float attack_ms, float decay_ms, float sustain_level, float release_ms);
    virtual void setPortamento(uint8_t synth_id, uint16_t portamento_ms);
    virtual void loadPreset(uint8_t synth_id, uint16_t preset_id, uint8_t num_voices = 8);
    virtual void sendAmyMessage(const char* message);

    // Oscillator & Voice Controls (Bank B)
    virtual void setOscDetune(uint8_t synth_id, float cents);
    virtual void setSubOscLevel(uint8_t synth_id, float level);
    virtual void setNoiseLevel(uint8_t synth_id, float level);
    virtual void setOscMix(uint8_t synth_id, float mix);

    // FM Synthesis Controls
    virtual void setFmModIndex(uint8_t osc_id, float mod_index);
    virtual void setFmFeedback(uint8_t osc_id, float feedback);
    virtual void setFmRatio(uint8_t osc_id, float ratio);
    virtual void setFmAlgorithm(uint8_t osc_id, uint8_t algo_id);

    // Built-in AMY Effects Controls
    virtual void setChorus(float depth, float rate, float level);
    virtual void setChorusMode(uint8_t mode);
    virtual void setReverb(float room_size, float damp, float mix);
    virtual void setReverbFreeze(bool freeze);
    bool reverbFreeze() const { return reverb_freeze_.load(std::memory_order_relaxed); }
    virtual void setDelay(float delay_ms, float feedback, float mix);
    virtual void setSendLevels(uint8_t osc_id, float reverb_send, float chorus_send, float echo_send);

    // Master Output & Protection
    virtual void setSoftLimiter(bool enable) { soft_limiter_enabled_.store(enable, std::memory_order_relaxed); }
    bool softLimiterEnabled() const { return soft_limiter_enabled_.load(std::memory_order_relaxed); }
    virtual void setMasterGain(float gain);
    float masterGain() const { return master_gain_.load(std::memory_order_relaxed); }
    virtual void setDrive(float drive);
    float drive() const { return drive_level_.load(std::memory_order_relaxed); }
    virtual void setMasterTone(float tone);
    float masterTone() const { return master_tone_.load(std::memory_order_relaxed); }

    // Oscilloscope sample capture for UI
    static constexpr size_t kScopeBufferSize = 128;
    static constexpr size_t kRawScopeBufferSize = 512; // stereo samples (256 frames * 2)
    void getScopeSamples(int16_t* dest, size_t max_count, size_t* out_count = nullptr) const override;

    // Baseline FM operator metadata captured by the synthesis owner. The
    // control/UI layer reads it to map soft-takeover knobs back to the timbre
    // that the currently loaded patch actually produces. Reading is lock-free;
    // only the synthesis owner updates it.
    bool fmBaseline(uint8_t& algorithm, float& feedback) const;
    // Called when a new patch is requested so the previous patch's baseline is
    // never observed while the new preset is still being materialized.
    void invalidateFmBaseline();

    // Monophonic Legato
    void setMonoMode(bool enable);
    bool isMonoMode() const { return mono_snapshot_.load(); }

private:
    friend struct AmyAdapterTestAccess;
    struct State;
    State* state_ = nullptr;
    bool initialized_ = false;
    static void workerRoutine(void* arg);
    bool serviceBlock(); // Single owner, also driven synchronously by host tests.
    // Host-test access to the synthesis owner's private render-time EWMA. Not
    // part of the runtime control path; the owner is the only runtime writer.
    uint64_t renderAverageUsForTest() const;
    void setRenderAverageUsForTest(uint64_t average_us);
    void submit(uint8_t type, uint8_t channel, uint16_t id,
                float a = 0, float b = 0, float c = 0, float d = 0,
                float e = 0, float f = 0, float g = 0);
    bool beginEngine(uint32_t sample_rate_hz);
    void endEngine();
    void executeNoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
    void executeNoteOff(uint8_t channel, uint8_t note);
    void executePitchBend(uint8_t channel, int16_t value);
    void executeControlChange(uint8_t channel, uint8_t controller, uint8_t value);
    void executeAllNotesOff();
    void executePanic();
    void executeFilter(uint8_t id, float cutoff, float resonance,
                       float env_amount, float key_tracking, float vel_tracking, uint8_t filter_type);
    void executeWaveform(uint8_t id, uint8_t wave);
    void executeEnvelope(uint8_t id, float a, float d, float s, float r);
    void executePortamento(uint8_t id, uint16_t ms);
    void executePreset(uint8_t id, uint16_t preset, uint8_t voices);
    void executeMessage(const char* message);
    void executeFmModIndex(uint8_t id, float value);
    void executeFmFeedback(uint8_t id, float value);
    void executeFmRatio(uint8_t id, float value);
    void executeFmAlgorithm(uint8_t id, uint8_t value);
    void executeChorus(float depth, float rate, float level);
    void executeChorusMode(uint8_t mode);
    void executeReverb(float room, float damp, float mix);
    void executeReverbFreeze(bool freeze);
    void executeDelay(float ms, float feedback, float mix);
    void executeDrive(float drive);
    void executeMasterTone(float tone);
    void executeOscDetune(uint8_t synth_id, float cents);
    void executeSubOscLevel(uint8_t synth_id, float level);
    void executeNoiseLevel(uint8_t synth_id, float level);
    void executeOscMix(uint8_t synth_id, float mix);
    int16_t* renderEngine();
    std::atomic<bool> mono_snapshot_{false};
    std::atomic<float> render_load_snapshot_{0};
    std::atomic<uint32_t> active_voices_{0};
    std::atomic<bool> soft_limiter_enabled_{true};
    std::atomic<float> master_gain_{1.0f};
    std::atomic<float> drive_level_{0.0f};
    std::atomic<float> master_tone_{0.0f};
    std::atomic<bool>  reverb_freeze_{false};
    uint8_t active_notes_[16][128]{};
    std::atomic<int16_t> scope_buffer_[kRawScopeBufferSize]{};

    static constexpr size_t kMonoStackCap = 16;
    uint8_t mono_stack_[kMonoStackCap]{};
    uint8_t mono_stack_size_{0};
    bool mono_mode_{false};

    struct FmOpSnapshot {
        float base_level = 0.0f;
        float base_logratio = 0.0f;
        float base_logfreq = 0.0f;
        bool  valid = false;
    };
    static constexpr size_t kMaxVoicesSnapshot = 32;
    static constexpr size_t kMaxOpsSnapshot = 6;
    FmOpSnapshot fm_base_ops_[kMaxVoicesSnapshot][kMaxOpsSnapshot]{};
    FmOpSnapshot fm_base_mod_source_[kMaxVoicesSnapshot]{};
    bool fm_snapshot_valid_{false};
    std::atomic<uint8_t> fm_baseline_algorithm_{1};
    std::atomic<float>   fm_baseline_feedback_{0.0f};
    std::atomic<bool>    fm_baseline_valid_{false};
    void captureFmBaseState(uint8_t synth_id);
};

} // namespace smk
