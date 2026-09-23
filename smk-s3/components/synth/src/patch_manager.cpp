#include "patch_manager.h"
#include "amy_adapter.h"
#include "ui_manager.h"
#include "clock_manager.h"
#include "arpeggiator.h"
#include "step_sequencer.h"
#include "esp_log.h"
#include <algorithm>
#include <cstring>
#include <cmath>

static const char* TAG = "PatchManager";

namespace smk {

PatchManager::PatchManager() {
    // Initialize with default factory patch 0
    active_patch_ = *FactoryPatches::getPatchById(0);
}

void PatchManager::applyActiveFilterState() {
    if (amy_adapter_) {
        amy_adapter_->setFilter(1,
            active_patch_.filter_cutoff,
            active_patch_.filter_res,
            active_filter_env_amt_,
            active_filter_key_track_,
            active_filter_vel_track_,
            active_filter_type_);
    }
}

void PatchManager::applyActiveChorusState() {
    if (!amy_adapter_) return;
    // 0=Off, 1=Classic, 2=Juno, 3=Ensemble, 4=Wide, 5=Vibrato
    static const struct {
        float base_depth;
        float rate;
        float base_level;
    } kChorusPresets[6] = {
        { 0.0f, 0.0f, 0.0f },  // 0: Off
        { 0.5f, 0.5f, 0.7f },  // 1: Classic
        { 0.8f, 0.6f, 0.85f }, // 2: Juno
        { 1.2f, 0.9f, 1.0f },  // 3: Ensemble
        { 1.5f, 0.4f, 0.9f },  // 4: Wide
        { 0.4f, 4.5f, 0.6f }   // 5: Vibrato
    };
    uint8_t mode = std::clamp<uint8_t>(fx_state_.chorus_mode, 0, 5);
    if (mode == 0) {
        amy_adapter_->setChorus(0.0f, 0.0f, 0.0f);
    } else {
        const auto& p = kChorusPresets[mode];
        float depth_mult = (fx_state_.chorus_depth > 0.001f) ? fx_state_.chorus_depth : 1.0f;
        amy_adapter_->setChorus(p.base_depth * depth_mult, p.rate, p.base_level * depth_mult);
    }
}

bool PatchManager::begin(AmyAdapter* amy_adapter, UIManager* ui_manager) {
    amy_adapter_ = amy_adapter;
    ui_manager_  = ui_manager;

    ESP_LOGI(TAG, "Initializing PatchManager with active patch: %s", active_patch_.name);

    // Apply default patch 0 to engine
    selectPatch(0);
    return true;
}

void PatchManager::nextKnobBank() {
    uint8_t current = static_cast<uint8_t>(active_bank_);
    setKnobBank(static_cast<KnobBank>((current + 1) % 5));
}

void PatchManager::setKnobBank(KnobBank bank) {
    if (active_bank_ == bank) return;
    active_bank_ = bank;
    soft_takeover_.resetAll();

    const char* bank_name = "MACROS";
    switch (active_bank_) {
        case KnobBank::BankA_Macros:     bank_name = "MACROS"; break;
        case KnobBank::BankB_Oscillator: bank_name = "OSC/FM"; break;
        case KnobBank::BankC_FilterEnv:  bank_name = "FLT/ENV"; break;
        case KnobBank::BankD_Effects:    bank_name = "FX"; break;
        case KnobBank::BankE_Sequencer:  bank_name = "SEQ/ARP"; break;
    }

    ESP_LOGI(TAG, "Switched active Knob Bank to %s", bank_name);

    if (ui_manager_) {
        ui_manager_->homeScreen().setKnobBankLabel(bank_name);
        ui_manager_->triggerParameterOverlay("KNOB BANK", bank_name, static_cast<float>(active_bank_), 0.0f, "", TakeoverStatus::Captured);
    }
}

void PatchManager::handleKnobInput(uint8_t knob_idx, float physical_val) {
    if (knob_idx >= 16) return;

    if (knob_idx < 8) {
        if (ui_manager_ && ui_manager_->homeScreen().homeKnobBankView() != HomeScreen::HomeKnobBankView::BankA_Macros) {
            ui_manager_->homeScreen().setHomeKnobBankView(HomeScreen::HomeKnobBankView::BankA_Macros);
        }

        if (active_bank_ == KnobBank::BankA_Macros) {
            setMacro(knob_idx, physical_val, true);
            return;
        }

        float saved_val = 64.0f;
        switch (active_bank_) {
            case KnobBank::BankB_Oscillator:
                if (knob_idx == 0) saved_val = active_patch_.osc_mix * 127.0f;
                else if (knob_idx == 1) saved_val = (active_patch_.wave_type / 8.0f) * 127.0f;
                else if (knob_idx == 2) saved_val = std::clamp((active_patch_.osc_detune + 100.0f) / 200.0f * 127.0f, 0.0f, 127.0f);
                else if (knob_idx == 3) saved_val = std::clamp(((active_patch_.transpose / 12.0f + 2.0f) / 4.0f) * 127.0f, 0.0f, 127.0f);
                else if (knob_idx == 4) saved_val = active_patch_.sub_level * 127.0f;
                else if (knob_idx == 5) saved_val = active_patch_.noise_level * 127.0f;
                break;
            case KnobBank::BankC_FilterEnv:
                switch (knob_idx) {
                    case 0: saved_val = std::clamp((active_patch_.filter_cutoff - 20.0f) / 18000.0f * 127.0f, 0.0f, 127.0f); break;
                    case 1: saved_val = std::clamp((active_patch_.filter_res - 0.5f) / 9.5f * 127.0f, 0.0f, 127.0f); break;
                    case 2: saved_val = std::clamp((active_filter_env_amt_ + 4.0f) / 8.0f * 127.0f, 0.0f, 127.0f); break;
                    case 3: saved_val = std::clamp((active_patch_.amp_attack - 1.0f) / 4999.0f * 127.0f, 0.0f, 127.0f); break;
                    case 4: saved_val = std::clamp((active_patch_.amp_decay - 1.0f) / 4999.0f * 127.0f, 0.0f, 127.0f); break;
                    case 5: saved_val = std::clamp(active_patch_.amp_sustain * 127.0f, 0.0f, 127.0f); break;
                    case 6: saved_val = std::clamp((active_patch_.amp_release - 1.0f) / 4999.0f * 127.0f, 0.0f, 127.0f); break;
                    case 7: saved_val = std::clamp(active_filter_key_track_ / 2.0f * 127.0f, 0.0f, 127.0f); break;
                }
                break;
            case KnobBank::BankD_Effects:
                switch (knob_idx) {
                    case 0: saved_val = std::clamp(fx_state_.chorus_mode / 5.0f * 127.0f, 0.0f, 127.0f); break;
                    case 1: saved_val = std::clamp((fx_state_.delay_time_ms - 10.0f) / 990.0f * 127.0f, 0.0f, 127.0f); break;
                    case 2: saved_val = std::clamp(fx_state_.delay_feedback / 0.95f * 127.0f, 0.0f, 127.0f); break;
                    case 3: saved_val = std::clamp(fx_state_.delay_mix * 127.0f, 0.0f, 127.0f); break;
                    case 4: saved_val = std::clamp(fx_state_.reverb_size * 127.0f, 0.0f, 127.0f); break;
                    case 5: saved_val = std::clamp(fx_state_.reverb_mix * 127.0f, 0.0f, 127.0f); break;
                    case 6: saved_val = std::clamp(fx_state_.drive * 127.0f, 0.0f, 127.0f); break;
                    case 7: saved_val = std::clamp((fx_state_.master_tone + 1.0f) / 2.0f * 127.0f, 0.0f, 127.0f); break;
                    default: break;
                }
                break;
            case KnobBank::BankE_Sequencer:
                if (knob_idx == 0 && clock_manager_) saved_val = std::clamp((clock_manager_->bpm() - 30.0f) / 270.0f * 127.0f, 0.0f, 127.0f);
                else if (knob_idx == 1 && sequencer_) saved_val = std::clamp(sequencer_->swing() / 75.0f * 127.0f, 0.0f, 127.0f);
                else if (knob_idx == 5 && sequencer_) saved_val = std::clamp((sequencer_->patternLength() - 1) / 15.0f * 127.0f, 0.0f, 127.0f);
                else if (knob_idx == 6) saved_val = std::clamp((active_patch_.transpose + 24.0f) / 48.0f * 127.0f, 0.0f, 127.0f);
                else if (knob_idx == 7 && sequencer_) saved_val = std::clamp(sequencer_->currentPattern() / 7.0f * 127.0f, 0.0f, 127.0f);
                break;
            default: break;
        }

        const char* param_name = "PARAM";
        const char* bank_label = "BANK B";
        float effective_val = physical_val;

        uint8_t takeover_id = static_cast<uint8_t>(active_bank_) * 8 + knob_idx;
        TakeoverStatus status = soft_takeover_.update(takeover_id, physical_val, saved_val, effective_val);
        float norm_val = std::clamp(effective_val / 127.0f, 0.0f, 1.0f);

    switch (active_bank_) {
        case KnobBank::BankB_Oscillator:
            if (active_patch_.wave_type == 8) {
                bank_label = "BANK B: FM OPERATORS";
                switch (knob_idx) {
                    case 0: { // FM Mod Index
                        param_name = "FM MOD INDEX";
                        float mod_index = norm_val * 10.0f;
                        if (amy_adapter_) amy_adapter_->setFmModIndex(1, mod_index);
                        break;
                    }
                    case 1: { // FM Operator Ratio
                        param_name = "FM OP RATIO";
                        float ratio = 0.5f + norm_val * 7.5f;
                        if (amy_adapter_) amy_adapter_->setFmRatio(1, ratio);
                        break;
                    }
                    case 2: { // FM Detune
                        param_name = "FM DETUNE";
                        float cents = (norm_val - 0.5f) * 50.0f;
                        if (amy_adapter_) amy_adapter_->setOscDetune(1, cents);
                        break;
                    }
                    case 3: { // FM Freq Multiplier
                        param_name = "FM FREQ MULT";
                        float mult = 1.0f + std::round(norm_val * 7.0f);
                        if (amy_adapter_) amy_adapter_->setFmRatio(1, mult);
                        break;
                    }
                    case 4: { // FM Mod Decay
                        param_name = "FM DECAY [N/A]";
                        // Operator-level envelope modulation not yet wired; do not corrupt global amp envelope
                        break;
                    }
                    case 5: { // FM Feedback
                        param_name = "FM FEEDBACK";
                        float feedback = norm_val * 0.16f;
                        if (amy_adapter_) amy_adapter_->setFmFeedback(1, feedback);
                        break;
                    }
                    case 6: { // FM Vibrato
                        param_name = "FM VIB [N/A]";
                        // True pitch LFO vibrato not yet wired; do not apply fake chorus
                        break;
                    }
                    case 7: { // DX7 Algorithm (1 .. 32)
                        param_name = "DX7 ALGO";
                        uint8_t algo = 1 + static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(norm_val * 31.0f)), 0, 31));
                        if (amy_adapter_) amy_adapter_->setFmAlgorithm(1, algo);
                        break;
                    }
                }
            } else {
                bank_label = "BANK B: OSC";
                switch (knob_idx) {
                    case 0:
                        param_name = "OSC MIX";
                        active_patch_.osc_mix = norm_val;
                        if (amy_adapter_) amy_adapter_->setOscMix(1, norm_val);
                        break;
                    case 1: // Waveform
                        {
                            uint8_t w = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(norm_val * 8.0f)), 0, 8));
                            active_patch_.wave_type = w;
                            if (amy_adapter_) amy_adapter_->setOscillatorWaveform(1, w);
                            param_name = waveTypeName(fromAmyWaveType(w));
                        }
                        break;
                    case 2:
                        param_name = "DETUNE";
                        active_patch_.osc_detune = (norm_val - 0.5f) * 100.0f;
                        if (amy_adapter_) amy_adapter_->setOscDetune(1, active_patch_.osc_detune);
                        break;
                    case 3: {
                        param_name = "OCTAVE";
                        int8_t oct = static_cast<int8_t>(std::round((norm_val - 0.5f) * 4.0f));
                        active_patch_.transpose = oct * 12;
                        break;
                    }
                    case 4:
                        param_name = "SUB OSC";
                        active_patch_.sub_level = norm_val;
                        if (amy_adapter_) amy_adapter_->setSubOscLevel(1, norm_val);
                        break;
                    case 5:
                        param_name = "NOISE LEVEL";
                        active_patch_.noise_level = norm_val;
                        if (amy_adapter_) amy_adapter_->setNoiseLevel(1, norm_val);
                        break;
                    case 6:
                        param_name = "FM AMOUNT";
                        if (amy_adapter_) amy_adapter_->setFmModIndex(1, norm_val * 10.0f);
                        break;
                    case 7:
                        param_name = "OSC MOD";
                        if (amy_adapter_) amy_adapter_->setChorus(norm_val * 0.5f, 1.5f, norm_val * 0.6f);
                        break;
                }
            }
            break;

        case KnobBank::BankC_FilterEnv:
            bank_label = "BANK C: FLT/ENV";
            switch (knob_idx) {
                case 0: // Cutoff
                    param_name = "CUTOFF FREQ";
                    active_patch_.filter_cutoff = 20.0f + norm_val * 18000.0f;
                    applyActiveFilterState();
                    break;
                case 1: // Resonance
                    param_name = "RESONANCE";
                    active_patch_.filter_res = 0.5f + norm_val * 9.5f;
                    applyActiveFilterState();
                    break;
                case 2: // ENV AMOUNT
                    param_name = "ENV AMOUNT";
                    active_filter_env_amt_ = (norm_val - 0.5f) * 8.0f;
                    active_patch_.filter_env_amount = active_filter_env_amt_;
                    applyActiveFilterState();
                    break;
                case 3: // Amp Attack
                    param_name = "AMP ATTACK";
                    active_patch_.amp_attack = 1.0f + norm_val * 4999.0f;
                    if (amy_adapter_) amy_adapter_->setEnvelope(1, active_patch_.amp_attack, active_patch_.amp_decay, active_patch_.amp_sustain, active_patch_.amp_release);
                    break;
                case 4: // Amp Decay
                    param_name = "AMP DECAY";
                    active_patch_.amp_decay = 1.0f + norm_val * 4999.0f;
                    if (amy_adapter_) amy_adapter_->setEnvelope(1, active_patch_.amp_attack, active_patch_.amp_decay, active_patch_.amp_sustain, active_patch_.amp_release);
                    break;
                case 5: // Amp Sustain
                    param_name = "AMP SUSTAIN";
                    active_patch_.amp_sustain = norm_val;
                    if (amy_adapter_) amy_adapter_->setEnvelope(1, active_patch_.amp_attack, active_patch_.amp_decay, active_patch_.amp_sustain, active_patch_.amp_release);
                    break;
                case 6: // Amp Release
                    param_name = "AMP RELEASE";
                    active_patch_.amp_release = 1.0f + norm_val * 4999.0f;
                    if (amy_adapter_) amy_adapter_->setEnvelope(1, active_patch_.amp_attack, active_patch_.amp_decay, active_patch_.amp_sustain, active_patch_.amp_release);
                    break;
                case 7: // KEY TRACKING
                    param_name = "KEY TRACKING";
                    active_filter_key_track_ = norm_val * 2.0f;
                    active_patch_.filter_key_tracking = active_filter_key_track_;
                    applyActiveFilterState();
                    break;
            }
            break;

        case KnobBank::BankD_Effects:
            bank_label = "BANK D: FX";
            switch (knob_idx) {
                case 0: {
                    param_name = "CHORUS MODE";
                    uint8_t mode = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(norm_val * 5.0f)), 0, 5));
                    fx_state_.chorus_mode = mode;
                    active_patch_.chorus_mode = mode;
                    applyActiveChorusState();
                    break;
                }
                case 1: {
                    param_name = "DELAY SYNC";
                    float delay_ms = 10.0f + norm_val * 990.0f;
                    if (clock_manager_ && clock_manager_->bpm() >= 30.0f) {
                        float beat_ms = 60000.0f / clock_manager_->bpm();
                        static const float kDivMultipliers[7] = { 0.25f, 0.3333f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f };
                        int div_idx = std::clamp(static_cast<int>(norm_val * 6.99f), 0, 6);
                        delay_ms = std::min(beat_ms * kDivMultipliers[div_idx], 1200.0f);
                    }
                    fx_state_.delay_time_ms = delay_ms;
                    if (amy_adapter_) amy_adapter_->setDelay(fx_state_.delay_time_ms, fx_state_.delay_feedback, fx_state_.delay_mix);
                    break;
                }
                case 2:
                    param_name = "DELAY FEEDBACK";
                    fx_state_.delay_feedback = norm_val * 0.95f;
                    if (amy_adapter_) amy_adapter_->setDelay(fx_state_.delay_time_ms, fx_state_.delay_feedback, fx_state_.delay_mix);
                    break;
                case 3:
                    param_name = "DELAY MIX";
                    fx_state_.delay_mix = norm_val;
                    if (amy_adapter_) amy_adapter_->setDelay(fx_state_.delay_time_ms, fx_state_.delay_feedback, fx_state_.delay_mix);
                    break;
                case 4:
                    param_name = "REVERB SIZE";
                    fx_state_.reverb_size = norm_val;
                    if (amy_adapter_) amy_adapter_->setReverb(fx_state_.reverb_size, 0.7f, fx_state_.reverb_mix);
                    break;
                case 5:
                    param_name = "REVERB MIX";
                    fx_state_.reverb_mix = norm_val;
                    if (amy_adapter_) amy_adapter_->setReverb(fx_state_.reverb_size, 0.7f, fx_state_.reverb_mix);
                    break;
                case 6:
                    param_name = "DRIVE LEVEL";
                    fx_state_.drive = norm_val;
                    active_patch_.drive_level = norm_val;
                    if (amy_adapter_) amy_adapter_->setDrive(norm_val);
                    break;
                case 7:
                    param_name = "MASTER TONE";
                    fx_state_.master_tone = (norm_val - 0.5f) * 2.0f;
                    active_patch_.master_tone = fx_state_.master_tone;
                    if (amy_adapter_) amy_adapter_->setMasterTone(fx_state_.master_tone);
                    break;
            }
            break;

        case KnobBank::BankE_Sequencer:
            bank_label = "BANK E: SEQ/ARP";
            switch (knob_idx) {
                case 0: // BPM
                    param_name = "GLOBAL BPM";
                    if (clock_manager_) {
                        float bpm = 30.0f + norm_val * 270.0f;
                        clock_manager_->setBpm(bpm);
                    }
                    break;
                case 1:
                    param_name = "SWING AMOUNT";
                    if (sequencer_) sequencer_->setSwing(norm_val * 75.0f);
                    break;
                case 2:
                    param_name = "GATE LENGTH";
                    if (sequencer_) {
                        uint8_t gate = 10 + static_cast<uint8_t>(norm_val * 90.0f);
                        for (size_t s = 0; s < StepSequencer::kMaxSteps; ++s) {
                            sequencer_->step(s).gate_percent = gate;
                        }
                    }
                    break;
                case 3:
                    param_name = "PROBABILITY";
                    if (sequencer_) {
                        uint8_t prob = static_cast<uint8_t>(norm_val * 100.0f);
                        for (size_t s = 0; s < StepSequencer::kMaxSteps; ++s) {
                            sequencer_->step(s).probability = prob;
                        }
                    }
                    break;
                case 4:
                    param_name = "RATCHET COUNT";
                    if (sequencer_) {
                        uint8_t ratchet = 1 + static_cast<uint8_t>(norm_val * 3.99f);
                        for (size_t s = 0; s < StepSequencer::kMaxSteps; ++s) {
                            sequencer_->step(s).ratchet = ratchet;
                        }
                    }
                    break;
                case 5:
                    param_name = "PATTERN LENGTH";
                    if (sequencer_) {
                        uint8_t len = 1 + static_cast<uint8_t>(norm_val * 15.0f);
                        sequencer_->setPatternLength(len);
                    }
                    break;
                case 6: {
                    param_name = "TRANSPOSE";
                    int8_t trans = static_cast<int8_t>(std::round((norm_val - 0.5f) * 48.0f));
                    active_patch_.transpose = trans;
                    break;
                }
                case 7:
                    param_name = "PATTERN SELECT";
                    if (sequencer_) {
                        uint8_t pat = static_cast<uint8_t>(norm_val * 7.99f);
                        sequencer_->selectPattern(pat);
                    }
                    break;
            }
            break;

        default:
            break;
    }

    if (ui_manager_) {
        ui_manager_->triggerParameterOverlay(param_name, bank_label, effective_val, saved_val, "", status);
    }
    return;
}

// ─────────────────────────────────────────────────────────────
// Physical Knobs Bank B (Knobs 9..16 -> b_idx 0..7)
// Direct Sound Engine & FX Performance Controls
// ─────────────────────────────────────────────────────────────
uint8_t b_idx = knob_idx - 8;
uint8_t takeover_id = 8 + b_idx;
const char* param_name = "ENGINE";
const char* bank_label = "BANK B: ENGINE";
float effective_val = physical_val;
TakeoverStatus status = TakeoverStatus::Captured;

switch (b_idx) {
    case 0: { // Knob B1: Cutoff Frequency (20Hz .. 18000Hz)
        param_name = "CUTOFF FREQ";
        float saved_val = std::clamp((active_patch_.filter_cutoff - 20.0f) / 18000.0f * 127.0f, 0.0f, 127.0f);
        status = soft_takeover_.update(takeover_id, physical_val, saved_val, effective_val);
        float norm = std::clamp(effective_val / 127.0f, 0.0f, 1.0f);
        active_patch_.filter_cutoff = 20.0f + norm * 18000.0f;
        applyActiveFilterState();
        break;
    }
    case 1: { // Knob B2: Resonance (0.5 .. 10.0)
        param_name = "RESONANCE";
        float saved_val = std::clamp((active_patch_.filter_res - 0.5f) / 9.5f * 127.0f, 0.0f, 127.0f);
        status = soft_takeover_.update(takeover_id, physical_val, saved_val, effective_val);
        float norm = std::clamp(effective_val / 127.0f, 0.0f, 1.0f);
        active_patch_.filter_res = 0.5f + norm * 9.5f;
        applyActiveFilterState();
        break;
    }
    case 2: { // Knob B3: Amp Attack (1ms .. 5000ms)
        param_name = "AMP ATTACK";
        float saved_val = std::clamp((active_patch_.amp_attack - 1.0f) / 4999.0f * 127.0f, 0.0f, 127.0f);
        status = soft_takeover_.update(takeover_id, physical_val, saved_val, effective_val);
        float norm = std::clamp(effective_val / 127.0f, 0.0f, 1.0f);
        active_patch_.amp_attack = 1.0f + norm * 4999.0f;
        if (amy_adapter_) amy_adapter_->setEnvelope(1, active_patch_.amp_attack, active_patch_.amp_decay, active_patch_.amp_sustain, active_patch_.amp_release);
        break;
    }
    case 3: { // Knob B4: Amp Release (1ms .. 5000ms)
        param_name = "AMP RELEASE";
        float saved_val = std::clamp((active_patch_.amp_release - 1.0f) / 4999.0f * 127.0f, 0.0f, 127.0f);
        status = soft_takeover_.update(takeover_id, physical_val, saved_val, effective_val);
        float norm = std::clamp(effective_val / 127.0f, 0.0f, 1.0f);
        active_patch_.amp_release = 1.0f + norm * 4999.0f;
        if (amy_adapter_) amy_adapter_->setEnvelope(1, active_patch_.amp_attack, active_patch_.amp_decay, active_patch_.amp_sustain, active_patch_.amp_release);
        break;
    }
    case 4: { // Knob B5: Chorus Depth (0% .. 100%)
        param_name = "CHORUS DEPTH";
        float saved_val = std::clamp(fx_state_.chorus_depth * 127.0f, 0.0f, 127.0f);
        status = soft_takeover_.update(takeover_id, physical_val, saved_val, effective_val);
        float norm = std::clamp(effective_val / 127.0f, 0.0f, 1.0f);
        fx_state_.chorus_depth = norm;
        applyActiveChorusState();
        break;
    }
    case 5: { // Knob B6: Delay Time (10ms .. 1000ms)
        param_name = "DELAY TIME";
        float saved_val = std::clamp((fx_state_.delay_time_ms - 10.0f) / 990.0f * 127.0f, 0.0f, 127.0f);
        status = soft_takeover_.update(takeover_id, physical_val, saved_val, effective_val);
        float norm = std::clamp(effective_val / 127.0f, 0.0f, 1.0f);
        fx_state_.delay_time_ms = 10.0f + norm * 990.0f;
        if (amy_adapter_) amy_adapter_->setDelay(fx_state_.delay_time_ms, fx_state_.delay_feedback, fx_state_.delay_mix);
        break;
    }
    case 6: { // Knob B7: Reverb Mix (0% .. 100%)
        param_name = "REVERB MIX";
        float saved_val = std::clamp(fx_state_.reverb_mix * 127.0f, 0.0f, 127.0f);
        status = soft_takeover_.update(takeover_id, physical_val, saved_val, effective_val);
        float norm = std::clamp(effective_val / 127.0f, 0.0f, 1.0f);
        fx_state_.reverb_mix = norm;
        if (amy_adapter_) amy_adapter_->setReverb(fx_state_.reverb_size, 0.7f, fx_state_.reverb_mix);
        break;
    }
    case 7: { // Knob B8: Master Tone / FM Feedback / Drive
        param_name = (active_patch_.wave_type == 8) ? "FM FEEDBACK" : "MASTER DRIVE";
        float saved_val = std::clamp(fx_state_.drive * 127.0f, 0.0f, 127.0f);
        status = soft_takeover_.update(takeover_id, physical_val, saved_val, effective_val);
        float norm = std::clamp(effective_val / 127.0f, 0.0f, 1.0f);
        fx_state_.drive = norm;
        active_patch_.drive_level = norm;
        if (active_patch_.wave_type == 8 && amy_adapter_) {
            amy_adapter_->setFmFeedback(1, std::clamp(norm * 0.16f, 0.0f, 0.16f));
        } else if (amy_adapter_) {
            amy_adapter_->setDrive(norm);
        }
        break;
    }
    default:
        break;
}

if (ui_manager_) {
    uint8_t eng_vals[8] = {
        static_cast<uint8_t>(std::clamp((active_patch_.filter_cutoff - 20.0f) / 18000.0f * 127.0f, 0.0f, 127.0f)),
        static_cast<uint8_t>(std::clamp((active_patch_.filter_res - 0.5f) / 9.5f * 127.0f, 0.0f, 127.0f)),
        static_cast<uint8_t>(std::clamp((active_patch_.amp_attack - 1.0f) / 4999.0f * 127.0f, 0.0f, 127.0f)),
        static_cast<uint8_t>(std::clamp((active_patch_.amp_release - 1.0f) / 4999.0f * 127.0f, 0.0f, 127.0f)),
        static_cast<uint8_t>(std::clamp(fx_state_.chorus_depth * 127.0f, 0.0f, 127.0f)),
        static_cast<uint8_t>(std::clamp((fx_state_.delay_time_ms - 10.0f) / 990.0f * 127.0f, 0.0f, 127.0f)),
        static_cast<uint8_t>(std::clamp(fx_state_.reverb_mix * 127.0f, 0.0f, 127.0f)),
        static_cast<uint8_t>(std::clamp(fx_state_.drive * 127.0f, 0.0f, 127.0f))
    };
    ui_manager_->homeScreen().setHomeKnobBankView(HomeScreen::HomeKnobBankView::BankB_Engine);
    ui_manager_->homeScreen().setEngineValues(eng_vals);
    ui_manager_->triggerParameterOverlay(param_name, bank_label, effective_val, 64.0f, "", status);
}
}

bool PatchManager::selectPatch(uint8_t patch_id) {
    const SynthPatch* p = FactoryPatches::getPatchById(patch_id);
    if (!p) {
        ESP_LOGE(TAG, "Patch ID %d not found", patch_id);
        return false;
    }

    active_patch_ = *p;
    active_patch_.crc32 = calculatePatchCrc32(active_patch_);

    ESP_LOGI(TAG, "Loaded Patch #%d [%s] (CRC32: 0x%08X)", 
             active_patch_.id, active_patch_.name, active_patch_.crc32);

    // Reset soft takeover states for all macros with the new saved values
    soft_takeover_.resetAll();
    for (uint8_t i = 0; i < 8; ++i) {
        soft_takeover_.reset(i, active_patch_.macros[i].current_val);
    }

    if (amy_adapter_) amy_adapter_->allNotesOff();

    applyPatchToEngine(active_patch_);

    // Update UI overlay and HomeScreen
    if (ui_manager_) {
        uint8_t m_vals[8];
        for (int i = 0; i < 8; ++i) m_vals[i] = static_cast<uint8_t>(active_patch_.macros[i].current_val);
        ui_manager_->homeScreen().setPatchInfo(active_patch_.id, active_patch_.name, "SYNTH");
        ui_manager_->homeScreen().setMacroValues(m_vals);
        ui_manager_->homeScreen().setActiveVoices(amy_adapter_ ? amy_adapter_->activeVoices() : 0, active_patch_.voice_count);
        ui_manager_->triggerParameterOverlay("PATCH LOAD", "BANK A", (float)active_patch_.id, 0.0f, active_patch_.name, TakeoverStatus::Captured);
    }

    return true;
}

bool PatchManager::selectPatchByIndex(size_t index) {
    const SynthPatch* p = FactoryPatches::getPatchByIndex(index);
    if (!p) return false;
    return selectPatch(p->id);
}

void PatchManager::nextPatch() {
    size_t count = FactoryPatches::count();
    if (count == 0) return;
    size_t current_idx = 0;
    for (size_t i = 0; i < count; ++i) {
        const SynthPatch* p = FactoryPatches::getPatchByIndex(i);
        if (p && p->id == active_patch_.id) {
            current_idx = i;
            break;
        }
    }
    selectPatchByIndex((current_idx + 1) % count);
}

void PatchManager::previousPatch() {
    size_t count = FactoryPatches::count();
    if (count == 0) return;
    size_t current_idx = 0;
    for (size_t i = 0; i < count; ++i) {
        const SynthPatch* p = FactoryPatches::getPatchByIndex(i);
        if (p && p->id == active_patch_.id) {
            current_idx = i;
            break;
        }
    }
    size_t prev_idx = (current_idx == 0) ? (count - 1) : (current_idx - 1);
    selectPatchByIndex(prev_idx);
}

void PatchManager::setMacro(uint8_t macro_idx, float physical_val, bool from_physical_knob) {
    if (macro_idx >= 8) return;

    auto& macro = active_patch_.macros[macro_idx];
    float effective_val = physical_val;
    TakeoverStatus status = TakeoverStatus::Captured;

    if (from_physical_knob) {
        status = soft_takeover_.update(macro_idx, physical_val, macro.current_val, effective_val);
    } else {
        macro.current_val = physical_val;
        soft_takeover_.reset(macro_idx, physical_val);
    }

    macro.current_val = effective_val;

    ESP_LOGD(TAG, "Macro %d [%s] -> Physical: %.1f, Effective: %.1f, Status: %d",
             macro_idx, macro.name, physical_val, effective_val, (int)status);

    // Live Motion Recording: Record parameter lock into sequencer when in Recording mode
    if (from_physical_knob && sequencer_ && sequencer_->isRecording()) {
        sequencer_->recordLiveMotion(macro_idx, effective_val);
    }

    applyMacroToEngine(macro_idx, effective_val);

    if (ui_manager_) {
        uint8_t m_vals[8];
        for (int i = 0; i < 8; ++i) m_vals[i] = static_cast<uint8_t>(active_patch_.macros[i].current_val);
        ui_manager_->homeScreen().setMacroValues(m_vals);
        ui_manager_->triggerParameterOverlay(macro.name, "SYNTH", effective_val, macro.default_val, "", status);
    }
}

void PatchManager::applyPatchToEngine(const SynthPatch& patch) {
    if (!amy_adapter_) return;

    // 1. Load built-in AMY preset (Juno presets 0..127, DX7 presets 128..255, PCM presets 256+)
    amy_adapter_->loadPreset(1, patch.engine_patch, patch.voice_count > 0 ? patch.voice_count : 8);

    // 2. Configure Monophonic Legato with Portamento if patch has mono_mode enabled
    if (patch.mono_mode) {
        amy_adapter_->setMonoMode(true);
        amy_adapter_->setPortamento(1, patch.portamento_ms > 0 ? patch.portamento_ms : 60);
    } else {
        amy_adapter_->setMonoMode(false);
        amy_adapter_->setPortamento(1, 0);
    }

    // 3. Configure Filter & Envelopes
    active_filter_env_amt_   = patch.filter_env_amount;
    active_filter_key_track_ = patch.filter_key_tracking;
    active_filter_vel_track_ = patch.filter_vel_tracking;
    active_filter_type_      = patch.filter_type;
    applyActiveFilterState();
    amy_adapter_->setEnvelope(1, patch.amp_attack, patch.amp_decay, patch.amp_sustain, patch.amp_release);

    // 4. Configure Oscillator & FX state
    amy_adapter_->setOscMix(1, patch.osc_mix);
    amy_adapter_->setOscDetune(1, patch.osc_detune);
    amy_adapter_->setSubOscLevel(1, patch.sub_level);
    amy_adapter_->setNoiseLevel(1, patch.noise_level);

    fx_state_.drive       = std::clamp(patch.drive_level, 0.0f, 1.0f);
    fx_state_.master_tone = patch.master_tone;
    fx_state_.chorus_mode = patch.chorus_mode;

    amy_adapter_->setDrive(fx_state_.drive);
    amy_adapter_->setMasterTone(fx_state_.master_tone);
    applyActiveChorusState();
    amy_adapter_->setReverbFreeze(patch.reverb_freeze != 0);

    // 5. Set default reverb & delay effect levels
    amy_adapter_->setReverb(fx_state_.reverb_size, 0.7f, fx_state_.reverb_mix);
    amy_adapter_->setDelay(fx_state_.delay_time_ms, fx_state_.delay_feedback, fx_state_.delay_mix);

    // 6. Update UI macro status without destructively overriding preset internals
    if (ui_manager_) {
        uint8_t m_vals[8];
        for (int i = 0; i < 8; ++i) m_vals[i] = static_cast<uint8_t>(patch.macros[i].current_val);
        ui_manager_->homeScreen().setMacroValues(m_vals);
    }
}

void PatchManager::applyMacroToEngine(uint8_t macro_idx, float effective_val) {
    if (!amy_adapter_) return;
    if (macro_idx >= 8) return;

    const auto& macro = active_patch_.macros[macro_idx];
    float norm_val = std::clamp(effective_val / 127.0f, 0.0f, 1.0f);

    for (uint8_t m = 0; m < macro.mapping_count; ++m) {
        const auto& map = macro.mappings[m];
        
        // Curve shaping: 0 = Linear, 1 = Exponential (v^2), 2 = Logarithmic (sqrt(v))
        float shaped_val = norm_val;
        if (map.curve_type == 1) {
            shaped_val = norm_val * norm_val;
        } else if (map.curve_type == 2) {
            shaped_val = std::sqrt(norm_val);
        }
        float target_val = map.min_val + shaped_val * (map.max_val - map.min_val);

        switch (map.param_type) {
            case 0: // Filter Cutoff (Subtractive/Juno)
                if (active_patch_.engine_patch == 0 || active_patch_.wave_type != 8) {
                    active_patch_.filter_cutoff = target_val;
                    applyActiveFilterState();
                }
                break;
            case 1: // Filter Res (Subtractive/Juno)
                if (active_patch_.engine_patch == 0 || active_patch_.wave_type != 8) {
                    active_patch_.filter_res = target_val;
                    applyActiveFilterState();
                }
                break;
            case 2: // Brightness
                if (active_patch_.wave_type != 8) { // Subtractive / Juno
                    active_patch_.filter_cutoff = target_val;
                    applyActiveFilterState();
                } else {
                    amy_adapter_->setFmModIndex(1, target_val * 0.001f);
                }
                break;
            case 3: // Amp Attack (Enabled across all presets)
                active_patch_.amp_attack = target_val;
                amy_adapter_->setEnvelope(1, target_val, active_patch_.amp_decay, active_patch_.amp_sustain, active_patch_.amp_release);
                break;
            case 4: // Amp Release (Enabled across all presets)
                active_patch_.amp_release = target_val;
                amy_adapter_->setEnvelope(1, active_patch_.amp_attack, active_patch_.amp_decay, active_patch_.amp_sustain, target_val);
                break;
            case 5: // Motion (Chorus)
                {
                    fx_state_.chorus_depth = std::clamp(shaped_val, 0.0f, 1.0f);
                    applyActiveChorusState();
                }
                break;
            case 6: // Reverb / Space Send Level
                {
                    fx_state_.reverb_mix = std::clamp(shaped_val * 0.6f, 0.0f, 0.6f);
                    amy_adapter_->setReverb(fx_state_.reverb_size, 0.7f, fx_state_.reverb_mix);
                }
                break;
            case 7: // Safe Feedback / Drive
                if (active_patch_.wave_type == 8) {
                    float safe_fb = std::clamp(shaped_val * 0.16f, 0.0f, 0.16f);
                    amy_adapter_->setFmFeedback(1, safe_fb);
                } else {
                    float drive_val = std::clamp(target_val, 0.0f, 1.0f);
                    fx_state_.drive = drive_val;
                    active_patch_.drive_level = drive_val;
                    amy_adapter_->setDrive(drive_val);
                }
                break;
            default:
                break;
        }
    }
}

} // namespace smk
