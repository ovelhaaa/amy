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
    active_bank_ = bank;
    soft_takeover_.resetAll();

    const char* bank_name = "BANK A: MACROS";
    switch (active_bank_) {
        case KnobBank::BankA_Macros:     bank_name = "BANK A: MACROS"; break;
        case KnobBank::BankB_Oscillator: bank_name = "BANK B: OSCILLATORS"; break;
        case KnobBank::BankC_FilterEnv:  bank_name = "BANK C: FILTER & ENV"; break;
        case KnobBank::BankD_Effects:    bank_name = "BANK D: EFFECTS"; break;
        case KnobBank::BankE_Sequencer:  bank_name = "BANK E: SEQ & ARP"; break;
    }

    ESP_LOGI(TAG, "Switched active Knob Bank to %s", bank_name);

    if (ui_manager_) {
        ui_manager_->triggerParameterOverlay("KNOB BANK", bank_name, static_cast<float>(active_bank_), 0.0f, "", TakeoverStatus::Captured);
    }
}

void PatchManager::handleKnobInput(uint8_t knob_idx, float physical_val) {
    if (knob_idx >= 8) return;

    if (active_bank_ == KnobBank::BankA_Macros) {
        setMacro(knob_idx, physical_val, true);
        return;
    }

    float norm_val = std::clamp(physical_val / 127.0f, 0.0f, 1.0f);
    const char* param_name = "PARAM";
    const char* bank_label = "BANK B";
    float effective_val = physical_val;

    uint8_t takeover_id = static_cast<uint8_t>(active_bank_) * 8 + knob_idx;
    TakeoverStatus status = soft_takeover_.update(takeover_id, physical_val, physical_val, effective_val);

    switch (active_bank_) {
        case KnobBank::BankB_Oscillator:
            bank_label = "BANK B: OSC";
            switch (knob_idx) {
                case 0: // Osc Mix
                    param_name = "OSC MIX";
                    break;
                case 1: // Waveform
                    param_name = "WAVEFORM";
                    active_patch_.wave_type = static_cast<uint8_t>(norm_val * 8.0f);
                    if (amy_adapter_) {
                        for (uint8_t v = 0; v < active_patch_.voice_count; ++v) {
                            amy_adapter_->setOscillatorWaveform(v, active_patch_.wave_type);
                        }
                    }
                    break;
                case 2: // Detune
                    param_name = "DETUNE";
                    break;
                case 3: // Octave
                    param_name = "OCTAVE";
                    break;
                case 4: // Sub Osc Level
                    param_name = "SUB OSC";
                    break;
                case 5: // Noise Level
                    param_name = "NOISE LEVEL";
                    break;
                case 6: // FM Amount
                    param_name = "FM AMOUNT";
                    break;
                case 7: // Osc Mod Depth
                    param_name = "OSC MOD";
                    break;
            }
            break;

        case KnobBank::BankC_FilterEnv:
            bank_label = "BANK C: FLT/ENV";
            switch (knob_idx) {
                case 0: // Cutoff
                    param_name = "CUTOFF FREQ";
                    active_patch_.filter_cutoff = 20.0f + norm_val * 18000.0f;
                    if (amy_adapter_) {
                        for (uint8_t v = 0; v < active_patch_.voice_count; ++v) {
                            amy_adapter_->setFilter(v, active_patch_.filter_cutoff, active_patch_.filter_res);
                        }
                    }
                    break;
                case 1: // Resonance
                    param_name = "RESONANCE";
                    active_patch_.filter_res = 0.5f + norm_val * 9.5f;
                    if (amy_adapter_) {
                        for (uint8_t v = 0; v < active_patch_.voice_count; ++v) {
                            amy_adapter_->setFilter(v, active_patch_.filter_cutoff, active_patch_.filter_res);
                        }
                    }
                    break;
                case 2: // Filter Env Amt
                    param_name = "ENV AMOUNT";
                    break;
                case 3: // Amp Attack
                    param_name = "AMP ATTACK";
                    active_patch_.amp_attack = 1.0f + norm_val * 4999.0f;
                    break;
                case 4: // Amp Decay
                    param_name = "AMP DECAY";
                    active_patch_.amp_decay = 1.0f + norm_val * 4999.0f;
                    break;
                case 5: // Amp Sustain
                    param_name = "AMP SUSTAIN";
                    active_patch_.amp_sustain = norm_val;
                    break;
                case 6: // Amp Release
                    param_name = "AMP RELEASE";
                    active_patch_.amp_release = 1.0f + norm_val * 4999.0f;
                    break;
                case 7: // Key Tracking
                    param_name = "KEY TRACKING";
                    break;
            }
            break;

        case KnobBank::BankD_Effects:
            bank_label = "BANK D: FX";
            switch (knob_idx) {
                case 0:
                    param_name = "CHORUS DEPTH";
                    if (amy_adapter_) amy_adapter_->setChorus(norm_val, 0.5f, norm_val * 0.7f);
                    break;
                case 1:
                    param_name = "DELAY TIME";
                    if (amy_adapter_) amy_adapter_->setDelay(10.0f + norm_val * 990.0f, 0.4f, 0.5f);
                    break;
                case 2:
                    param_name = "DELAY FEEDBACK";
                    if (amy_adapter_) amy_adapter_->setDelay(250.0f, norm_val * 0.95f, 0.5f);
                    break;
                case 3:
                    param_name = "DELAY MIX";
                    if (amy_adapter_) amy_adapter_->setDelay(250.0f, 0.4f, norm_val);
                    break;
                case 4:
                    param_name = "REVERB SIZE";
                    if (amy_adapter_) amy_adapter_->setReverb(norm_val, 0.5f, 0.5f);
                    break;
                case 5:
                    param_name = "REVERB MIX";
                    if (amy_adapter_) amy_adapter_->setReverb(0.7f, 0.5f, norm_val);
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

    applyPatchToEngine(active_patch_);

    // Update UI overlay
    if (ui_manager_) {
        ui_manager_->triggerParameterOverlay("PATCH LOAD", "BANK A", (float)active_patch_.id, 0.0f, active_patch_.name, TakeoverStatus::Captured);
    }

    return true;
}

bool PatchManager::selectPatchByIndex(size_t index) {
    const SynthPatch* p = FactoryPatches::getPatchByIndex(index);
    if (!p) return false;
    return selectPatch(p->id);
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

    applyMacroToEngine(macro_idx, effective_val);

    if (ui_manager_) {
        ui_manager_->triggerParameterOverlay(macro.name, "LAYER A", effective_val, macro.default_val, "", status);
    }
}

void PatchManager::applyPatchToEngine(const SynthPatch& patch) {
    if (!amy_adapter_) return;

    // Configure voices and parameters in AMY engine
    for (uint8_t v = 0; v < patch.voice_count; ++v) {
        amy_adapter_->setOscillatorWaveform(v, patch.wave_type);
        amy_adapter_->setFilter(v, patch.filter_cutoff, patch.filter_res);
        amy_adapter_->setEnvelope(v, patch.amp_attack, patch.amp_decay, patch.amp_sustain, patch.amp_release);
    }

    // Apply all 8 macros
    for (uint8_t i = 0; i < 8; ++i) {
        applyMacroToEngine(i, patch.macros[i].current_val);
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
            case 0: // Filter Cutoff
                active_patch_.filter_cutoff = target_val;
                for (uint8_t v = 0; v < active_patch_.voice_count; ++v) {
                    amy_adapter_->setFilter(v, target_val, active_patch_.filter_res);
                }
                break;
            case 1: // Filter Res
                active_patch_.filter_res = target_val;
                for (uint8_t v = 0; v < active_patch_.voice_count; ++v) {
                    amy_adapter_->setFilter(v, active_patch_.filter_cutoff, target_val);
                }
                break;
            case 2: // Osc Waveform
                active_patch_.wave_type = (uint8_t)target_val;
                for (uint8_t v = 0; v < active_patch_.voice_count; ++v) {
                    amy_adapter_->setOscillatorWaveform(v, (uint8_t)target_val);
                }
                break;
            case 3: // Amp Attack
                active_patch_.amp_attack = target_val;
                for (uint8_t v = 0; v < active_patch_.voice_count; ++v) {
                    amy_adapter_->setEnvelope(v, target_val, active_patch_.amp_decay, active_patch_.amp_sustain, active_patch_.amp_release);
                }
                break;
            case 4: // Amp Release
                active_patch_.amp_release = target_val;
                for (uint8_t v = 0; v < active_patch_.voice_count; ++v) {
                    amy_adapter_->setEnvelope(v, active_patch_.amp_attack, active_patch_.amp_decay, active_patch_.amp_sustain, target_val);
                }
                break;
            default:
                break;
        }
    }
}

} // namespace smk
