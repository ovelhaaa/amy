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
                if (knob_idx == 1) saved_val = (active_patch_.wave_type / 8.0f) * 127.0f;
                break;
            case KnobBank::BankC_FilterEnv:
                switch (knob_idx) {
                    case 0: saved_val = std::clamp((active_patch_.filter_cutoff - 20.0f) / 18000.0f * 127.0f, 0.0f, 127.0f); break;
                    case 1: saved_val = std::clamp((active_patch_.filter_res - 0.5f) / 9.5f * 127.0f, 0.0f, 127.0f); break;
                    case 3: saved_val = std::clamp((active_patch_.amp_attack - 1.0f) / 4999.0f * 127.0f, 0.0f, 127.0f); break;
                    case 4: saved_val = std::clamp((active_patch_.amp_decay - 1.0f) / 4999.0f * 127.0f, 0.0f, 127.0f); break;
                    case 5: saved_val = std::clamp(active_patch_.amp_sustain * 127.0f, 0.0f, 127.0f); break;
                    case 6: saved_val = std::clamp((active_patch_.amp_release - 1.0f) / 4999.0f * 127.0f, 0.0f, 127.0f); break;
                }
                break;
            case KnobBank::BankD_Effects:
                if (knob_idx >= 1 && knob_idx <= 5) saved_val = (float)bank_b_fx_values_[knob_idx];
                break;
            case KnobBank::BankE_Sequencer:
                if (knob_idx == 0 && clock_manager_) saved_val = std::clamp((clock_manager_->bpm() - 30.0f) / 270.0f * 127.0f, 0.0f, 127.0f);
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
                    case 2: param_name = "FM DETUNE"; break;
                    case 3: param_name = "FM FREQ MULT"; break;
                    case 4: param_name = "FM MOD DECAY"; break;
                    case 5: { // FM Feedback
                        param_name = "FM FEEDBACK";
                        float feedback = norm_val * 0.16f;
                        if (amy_adapter_) amy_adapter_->setFmFeedback(1, feedback);
                        break;
                    }
                    case 6: { // FM Vibrato
                        param_name = "FM VIBRATO";
                        float vibrato = norm_val * 0.2f;
                        if (amy_adapter_) amy_adapter_->setFmModIndex(1, vibrato);
                        break;
                    }
                    case 7: { // DX7 Algorithm
                        param_name = "DX7 ALGO";
                        uint8_t algo = static_cast<uint8_t>(norm_val * 31.0f);
                        if (amy_adapter_) amy_adapter_->setFmAlgorithm(1, algo);
                        break;
                    }
                }
            } else {
                bank_label = "BANK B: OSC";
                switch (knob_idx) {
                    case 0: param_name = "OSC MIX"; break;
                    case 1: // Waveform
                        param_name = "WAVEFORM";
                        active_patch_.wave_type = static_cast<uint8_t>(norm_val * 8.0f);
                        if (amy_adapter_) amy_adapter_->setOscillatorWaveform(1, active_patch_.wave_type);
                        break;
                    case 2: param_name = "DETUNE"; break;
                    case 3: param_name = "OCTAVE"; break;
                    case 4: param_name = "SUB OSC"; break;
                    case 5: param_name = "NOISE LEVEL"; break;
                    case 6: param_name = "FM AMOUNT"; break;
                    case 7: param_name = "OSC MOD"; break;
                }
            }
            break;

        case KnobBank::BankC_FilterEnv:
            bank_label = "BANK C: FLT/ENV";
            switch (knob_idx) {
                case 0: // Cutoff
                    param_name = "CUTOFF FREQ";
                    active_patch_.filter_cutoff = 20.0f + norm_val * 18000.0f;
                    if (amy_adapter_) amy_adapter_->setFilter(1, active_patch_.filter_cutoff, active_patch_.filter_res);
                    break;
                case 1: // Resonance
                    param_name = "RESONANCE";
                    active_patch_.filter_res = 0.5f + norm_val * 9.5f;
                    if (amy_adapter_) amy_adapter_->setFilter(1, active_patch_.filter_cutoff, active_patch_.filter_res);
                    break;
                case 2: param_name = "ENV AMOUNT"; break;
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
                case 7: param_name = "KEY TRACKING"; break;
            }
            break;

        case KnobBank::BankD_Effects:
            bank_label = "BANK D: FX";
            switch (knob_idx) {
                case 0:
                    param_name = "CHORUS DEPTH";
                    if (amy_adapter_) amy_adapter_->setChorus(norm_val, 0.5f, norm_val * 0.7f);
                    break;
                case 1: {
                    param_name = "DELAY SYNC";
                    bank_b_fx_values_[1] = static_cast<uint8_t>(effective_val);
                    float delay_ms = 10.0f + norm_val * 990.0f;
                    if (clock_manager_ && clock_manager_->bpm() >= 30.0f) {
                        float beat_ms = 60000.0f / clock_manager_->bpm();
                        static const float kDivMultipliers[7] = { 0.25f, 0.3333f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f };
                        int div_idx = std::clamp(static_cast<int>(norm_val * 6.99f), 0, 6);
                        delay_ms = std::min(beat_ms * kDivMultipliers[div_idx], 1200.0f);
                    }
                    if (amy_adapter_) amy_adapter_->setDelay(delay_ms, (float)bank_b_fx_values_[2] / 127.0f * 0.95f, (float)bank_b_fx_values_[3] / 127.0f);
                    break;
                }
                case 2:
                    param_name = "DELAY FEEDBACK";
                    bank_b_fx_values_[2] = static_cast<uint8_t>(effective_val);
                    if (amy_adapter_) {
                        float delay_ms = 10.0f + (float)bank_b_fx_values_[1] / 127.0f * 990.0f;
                        if (clock_manager_ && clock_manager_->bpm() >= 30.0f) {
                            float beat_ms = 60000.0f / clock_manager_->bpm();
                            static const float kDivMultipliers[7] = { 0.25f, 0.3333f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f };
                            int div_idx = std::clamp(static_cast<int>((float)bank_b_fx_values_[1] / 127.0f * 6.99f), 0, 6);
                            delay_ms = std::min(beat_ms * kDivMultipliers[div_idx], 1200.0f);
                        }
                        amy_adapter_->setDelay(delay_ms, norm_val * 0.95f, (float)bank_b_fx_values_[3] / 127.0f);
                    }
                    break;
                case 3:
                    param_name = "DELAY MIX";
                    bank_b_fx_values_[3] = static_cast<uint8_t>(effective_val);
                    if (amy_adapter_) {
                        float delay_ms = 10.0f + (float)bank_b_fx_values_[1] / 127.0f * 990.0f;
                        if (clock_manager_ && clock_manager_->bpm() >= 30.0f) {
                            float beat_ms = 60000.0f / clock_manager_->bpm();
                            static const float kDivMultipliers[7] = { 0.25f, 0.3333f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f };
                            int div_idx = std::clamp(static_cast<int>((float)bank_b_fx_values_[1] / 127.0f * 6.99f), 0, 6);
                            delay_ms = std::min(beat_ms * kDivMultipliers[div_idx], 1200.0f);
                        }
                        amy_adapter_->setDelay(delay_ms, (float)bank_b_fx_values_[2] / 127.0f * 0.95f, norm_val);
                    }
                    break;
                case 4:
                    param_name = "REVERB SIZE";
                    bank_b_fx_values_[4] = static_cast<uint8_t>(effective_val);
                    if (amy_adapter_) amy_adapter_->setReverb(norm_val, 0.7f, (float)bank_b_fx_values_[5] / 127.0f);
                    break;
                case 5:
                    param_name = "REVERB MIX";
                    bank_b_fx_values_[5] = static_cast<uint8_t>(effective_val);
                    if (amy_adapter_) amy_adapter_->setReverb((float)bank_b_fx_values_[4] / 127.0f, 0.7f, norm_val);
                    break;
                case 6:
                    param_name = "DRIVE LEVEL";
                    break;
                case 7:
                    param_name = "MASTER TONE";
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
                case 1: param_name = "SWING AMOUNT"; break;
                case 2: param_name = "GATE LENGTH"; break;
                case 3: param_name = "PROBABILITY"; break;
                case 4: param_name = "RATCHET COUNT"; break;
                case 5: param_name = "PATTERN LENGTH"; break;
                case 6: param_name = "TRANSPOSE"; break;
                case 7: param_name = "PATTERN SELECT"; break;
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
        if (amy_adapter_) amy_adapter_->setFilter(1, active_patch_.filter_cutoff, active_patch_.filter_res);
        break;
    }
    case 1: { // Knob B2: Resonance (0.5 .. 10.0)
        param_name = "RESONANCE";
        float saved_val = std::clamp((active_patch_.filter_res - 0.5f) / 9.5f * 127.0f, 0.0f, 127.0f);
        status = soft_takeover_.update(takeover_id, physical_val, saved_val, effective_val);
        float norm = std::clamp(effective_val / 127.0f, 0.0f, 1.0f);
        active_patch_.filter_res = 0.5f + norm * 9.5f;
        if (amy_adapter_) amy_adapter_->setFilter(1, active_patch_.filter_cutoff, active_patch_.filter_res);
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
        status = soft_takeover_.update(takeover_id, physical_val, (float)bank_b_fx_values_[0], effective_val);
        bank_b_fx_values_[0] = static_cast<uint8_t>(std::clamp(effective_val, 0.0f, 127.0f));
        float norm = std::clamp(effective_val / 127.0f, 0.0f, 1.0f);
        if (amy_adapter_) amy_adapter_->setChorus(norm * 0.8f, 0.5f, norm * 0.7f);
        break;
    }
    case 5: { // Knob B6: Delay Time (10ms .. 1000ms)
        param_name = "DELAY TIME";
        status = soft_takeover_.update(takeover_id, physical_val, (float)bank_b_fx_values_[1], effective_val);
        bank_b_fx_values_[1] = static_cast<uint8_t>(std::clamp(effective_val, 0.0f, 127.0f));
        float norm = std::clamp(effective_val / 127.0f, 0.0f, 1.0f);
        if (amy_adapter_) amy_adapter_->setDelay(10.0f + norm * 990.0f, 0.4f, 0.5f);
        break;
    }
    case 6: { // Knob B7: Reverb Mix (0% .. 100%)
        param_name = "REVERB MIX";
        status = soft_takeover_.update(takeover_id, physical_val, (float)bank_b_fx_values_[2], effective_val);
        bank_b_fx_values_[2] = static_cast<uint8_t>(std::clamp(effective_val, 0.0f, 127.0f));
        float norm = std::clamp(effective_val / 127.0f, 0.0f, 1.0f);
        if (amy_adapter_) amy_adapter_->setReverb(0.7f, 0.5f, norm * 0.8f);
        break;
    }
    case 7: { // Knob B8: Master Tone / FM Feedback
        param_name = "MASTER DRIVE";
        status = soft_takeover_.update(takeover_id, physical_val, (float)bank_b_fx_values_[3], effective_val);
        bank_b_fx_values_[3] = static_cast<uint8_t>(std::clamp(effective_val, 0.0f, 127.0f));
        float norm = std::clamp(effective_val / 127.0f, 0.0f, 1.0f);
        if (active_patch_.wave_type == 8 && amy_adapter_) {
            amy_adapter_->setFmFeedback(1, std::clamp(norm * 0.16f, 0.0f, 0.16f));
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
        bank_b_fx_values_[0],
        bank_b_fx_values_[1],
        bank_b_fx_values_[2],
        bank_b_fx_values_[3]
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

    // 3. Set clean default effect levels (prevent noise leak / stale reverb accumulation)
    amy_adapter_->setReverb(0.7f, 0.7f, 0.0f);
    amy_adapter_->setChorus(0.0f, 0.0f, 0.0f);
    amy_adapter_->setDelay(0.0f, 0.0f, 0.0f);

    // 4. Update UI macro status without destructively overriding preset internals
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
        float target_val = map.min_val + norm_val * (map.max_val - map.min_val);

        switch (map.param_type) {
            case 0: // Filter Cutoff (Subtractive/Juno)
                if (active_patch_.engine_patch == 0 || active_patch_.wave_type != 8) {
                    active_patch_.filter_cutoff = target_val;
                    amy_adapter_->setFilter(1, target_val, active_patch_.filter_res);
                }
                break;
            case 1: // Filter Res (Subtractive/Juno)
                if (active_patch_.engine_patch == 0 || active_patch_.wave_type != 8) {
                    active_patch_.filter_res = target_val;
                    amy_adapter_->setFilter(1, active_patch_.filter_cutoff, target_val);
                }
                break;
            case 2: // Brightness
                if (active_patch_.wave_type != 8) { // Subtractive / Juno
                    active_patch_.filter_cutoff = target_val;
                    amy_adapter_->setFilter(1, target_val, active_patch_.filter_res);
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
                    float chorus_level = std::clamp(norm_val * 0.4f, 0.0f, 0.4f);
                    amy_adapter_->setChorus(chorus_level * 0.8f, 0.5f, chorus_level);
                }
                break;
            case 6: // Reverb / Space Send Level
                {
                    float reverb_level = std::clamp(norm_val * 0.45f, 0.0f, 0.45f);
                    amy_adapter_->setReverb(0.7f, 0.5f, reverb_level);
                }
                break;
            case 7: // Safe Feedback / Drive
                if (active_patch_.wave_type == 8) {
                    float safe_fb = std::clamp(norm_val * 0.16f, 0.0f, 0.16f);
                    amy_adapter_->setFmFeedback(1, safe_fb);
                }
                break;
            default:
                break;
        }
    }
}

} // namespace smk
