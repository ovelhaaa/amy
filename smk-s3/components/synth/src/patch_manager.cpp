#include "patch_manager.h"
#include "storage_manager.h"
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

void PatchManager::setFilterType(uint8_t filter_type) {
    active_filter_type_ = filter_type;
    active_patch_.filter_type = filter_type;
    applyActiveFilterState();
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
        float depth_mult = std::clamp(fx_state_.chorus_depth, 0.0f, 1.0f);
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

void PatchManager::syncFmStateFromBaseline() {
    if (fm_state_.initialized || !amy_adapter_) return;
    uint8_t algorithm = 1;
    float feedback = 0.0f;
    if (amy_adapter_->fmBaseline(algorithm, feedback)) {
        fm_state_.algorithm = algorithm;
        fm_state_.feedback = feedback;
        // Seed the manual FM base from the real preset. A neutral macro then
        // restores the preset's own feedback/algorithm instead of assuming 0/1.
        // Bank B paths call this before writing their knob value, so a manual
        // feedback edit still wins over the preset seed.
        manual_state_.fm_algorithm = algorithm;
        manual_state_.fm_feedback = feedback;
        fm_state_.initialized = true;
    }
    // If the engine has not materialized the preset yet, keep the neutral
    // defaults (1x/everything centered). Absolute FM writes are never issued
    // from here, so leaving the state uninitialized is safe.
}

MacroMappingMode PatchManager::macroMappingMode() const {
    return classifyMacroMappings(active_patch_);
}

void PatchManager::captureManualControlState() {
    manual_state_ = ManualControlState{};
    manual_state_.filter_cutoff  = active_patch_.filter_cutoff;
    manual_state_.filter_res     = active_patch_.filter_res;
    manual_state_.filter_env     = active_filter_env_amt_;
    manual_state_.amp_attack     = active_patch_.amp_attack;
    manual_state_.amp_decay      = active_patch_.amp_decay;
    manual_state_.amp_sustain    = active_patch_.amp_sustain;
    manual_state_.amp_release    = active_patch_.amp_release;
    manual_state_.osc_detune     = active_patch_.osc_detune;
    manual_state_.chorus_depth   = fx_state_.chorus_depth;
    manual_state_.delay_time_ms  = fx_state_.delay_time_ms;
    manual_state_.delay_feedback = fx_state_.delay_feedback;
    manual_state_.delay_mix      = fx_state_.delay_mix;
    manual_state_.reverb_size    = fx_state_.reverb_size;
    manual_state_.reverb_mix     = fx_state_.reverb_mix;
    manual_state_.drive          = fx_state_.drive;
    manual_state_.master_tone    = fx_state_.master_tone;
    // FM manual base starts centered. Feedback/algorithm are seeded lazily from
    // the real preset in syncFmStateFromBaseline() once it has materialized.
    manual_state_.fm_mod_factor   = 1.0f;
    manual_state_.fm_ratio_factor = 1.0f;
    manual_state_.fm_detune_cents = 0.0f;
    manual_state_.fm_freq_mult    = 1.0f;
    manual_state_.fm_feedback     = 0.0f;
    manual_state_.fm_algorithm    = 1;
}

void PatchManager::recomputeMacroTargets() {
    if (!amy_adapter_) return;

    const PatchFamily family = activeFamily();
    const bool is_fm = (family == PatchFamily::FM);
    const bool supports_env = supportsGenericAmpEnvelope(family);
    const bool supports_osc = supportsSubtractiveOscControls(family);

    // The engine materializes the preset asynchronously; pull the FM baseline
    // as soon as it is available so the manual feedback base is the real preset
    // feedback, never assumed to start at zero.
    if (is_fm) {
        syncFmStateFromBaseline();
    }

    // Accumulators default to the identity contribution.
    float cutoff_oct     = 0.0f;
    float res_factor     = 1.0f;
    float fenv_off       = 0.0f;
    float atk_factor     = 1.0f;
    float dec_factor     = 1.0f;
    float sus_off        = 0.0f;
    float rel_factor     = 1.0f;
    float detune_off     = 0.0f;
    float chorus_off     = 0.0f;
    float reverb_off     = 0.0f;
    float delay_off      = 0.0f;
    float drive_off      = 0.0f;
    float tone_off       = 0.0f;
    float fm_mod_factor  = 1.0f;
    float fm_ratio_factor = 1.0f;
    float fm_detune_off  = 0.0f;
    float fm_fb_off      = 0.0f;

    for (uint8_t i = 0; i < 8; ++i) {
        const auto& macro = active_patch_.macros[i];
        const float norm    = std::clamp(macro.current_val / 127.0f, 0.0f, 1.0f);
        const float neutral = std::clamp(macro.default_val / 127.0f, 0.0f, 1.0f);
        const float t = macroBipolar(norm, neutral);

        for (uint8_t m = 0; m < macro.mapping_count && m < 4; ++m) {
            const auto& map = macro.mappings[m];
            if (isLegacyMacroTarget(map.param_type)) continue;
            const float st = macroShapeBipolar(t, map.curve_type);
            switch (static_cast<MacroTarget>(map.param_type)) {
                case MacroTarget::FilterCutoffRelative:
                    cutoff_oct += macroOffset(st, map.min_val, map.max_val); break;
                case MacroTarget::FilterResRelative:
                    res_factor *= macroFactor(st, map.min_val, map.max_val); break;
                case MacroTarget::FilterEnvRelative:
                    fenv_off += macroOffset(st, map.min_val, map.max_val); break;
                case MacroTarget::AmpAttackRelative:
                    atk_factor *= macroFactor(st, map.min_val, map.max_val); break;
                case MacroTarget::AmpDecayRelative:
                    dec_factor *= macroFactor(st, map.min_val, map.max_val); break;
                case MacroTarget::AmpSustainRelative:
                    sus_off += macroOffset(st, map.min_val, map.max_val); break;
                case MacroTarget::AmpReleaseRelative:
                    rel_factor *= macroFactor(st, map.min_val, map.max_val); break;
                case MacroTarget::OscDetuneRelative:
                    detune_off += macroOffset(st, map.min_val, map.max_val); break;
                case MacroTarget::ChorusDepth:
                    chorus_off += macroOffset(st, map.min_val, map.max_val); break;
                case MacroTarget::ReverbMix:
                    reverb_off += macroOffset(st, map.min_val, map.max_val); break;
                case MacroTarget::DelayMix:
                    delay_off += macroOffset(st, map.min_val, map.max_val); break;
                case MacroTarget::DriveRelative:
                    drive_off += macroOffset(st, map.min_val, map.max_val); break;
                case MacroTarget::MasterToneRelative:
                    tone_off += macroOffset(st, map.min_val, map.max_val); break;
                case MacroTarget::FmModIndexRelative:
                    fm_mod_factor *= macroFactor(st, map.min_val, map.max_val); break;
                case MacroTarget::FmRatioRelative:
                    fm_ratio_factor *= macroFactor(st, map.min_val, map.max_val); break;
                case MacroTarget::FmDetuneRelative:
                    fm_detune_off += macroOffset(st, map.min_val, map.max_val); break;
                case MacroTarget::FmFeedbackRelative:
                    fm_fb_off += macroOffset(st, map.min_val, map.max_val); break;
                default:
                    break;
            }
        }
    }

    // Final target state = manual base composed with the macro contribution,
    // clamped to the safe engine ranges. The manual state is never modified
    // here, so a neutral macro always reproduces the manual value exactly.
    const float new_cutoff = std::clamp(manual_state_.filter_cutoff * std::exp2(cutoff_oct),
                                        control_ranges::kCutoffMinHz, control_ranges::kCutoffMaxHz);
    const float new_res    = std::clamp(manual_state_.filter_res * res_factor,
                                        control_ranges::kResonanceMin, control_ranges::kResonanceMax);
    const float new_fenv   = std::clamp(manual_state_.filter_env + fenv_off, -4.0f, 4.0f);
    const float new_atk    = std::clamp(manual_state_.amp_attack * atk_factor,
                                        control_ranges::kEnvelopeMinMs, control_ranges::kEnvelopeMaxMs);
    const float new_dec    = std::clamp(manual_state_.amp_decay * dec_factor,
                                        control_ranges::kEnvelopeMinMs, control_ranges::kEnvelopeMaxMs);
    const float new_sus    = std::clamp(manual_state_.amp_sustain + sus_off, 0.0f, 1.0f);
    const float new_rel    = std::clamp(manual_state_.amp_release * rel_factor,
                                        control_ranges::kEnvelopeMinMs, control_ranges::kEnvelopeMaxMs);
    const float new_detune = std::clamp(manual_state_.osc_detune + detune_off, -100.0f, 100.0f);
    const float new_chorus = std::clamp(manual_state_.chorus_depth + chorus_off, 0.0f, 1.0f);
    const float new_reverb = std::clamp(manual_state_.reverb_mix + reverb_off, 0.0f, 1.0f);
    const float new_delay  = std::clamp(manual_state_.delay_mix + delay_off, 0.0f, 1.0f);
    const float new_drive  = std::clamp(manual_state_.drive + drive_off, 0.0f, 1.0f);
    const float new_tone   = std::clamp(manual_state_.master_tone + tone_off, -1.0f, 1.0f);

    // FM final values: manual base multiplied/offset by the macro contribution.
    // freq_mult and the discrete ratio are folded in exactly once, and the
    // algorithm is never touched by macros.
    const float new_fm_mod     = std::clamp(manual_state_.fm_mod_factor * fm_mod_factor, 0.0f, 8.0f);
    const float new_fm_ratio   = manual_state_.fm_ratio_factor * manual_state_.fm_freq_mult * fm_ratio_factor;
    const float new_fm_detune  = manual_state_.fm_detune_cents + fm_detune_off;
    const float new_fm_fb      = std::clamp(manual_state_.fm_feedback + fm_fb_off, 0.0f, 0.16f);

    const float eps = 1e-4f;

    // Filter: one command when any filter parameter moved.
    bool filter_changed = false;
    if (std::fabs(new_cutoff - active_patch_.filter_cutoff) > eps) {
        active_patch_.filter_cutoff = new_cutoff; filter_changed = true;
    }
    if (std::fabs(new_res - active_patch_.filter_res) > eps) {
        active_patch_.filter_res = new_res; filter_changed = true;
    }
    if (std::fabs(new_fenv - active_filter_env_amt_) > eps) {
        active_filter_env_amt_ = new_fenv;
        active_patch_.filter_env_amount = new_fenv;
        filter_changed = true;
    }
    if (filter_changed) applyActiveFilterState();

    // Generic amp envelope: never issued for FM/DX7, whose operators own it.
    if (supports_env) {
        bool env_changed = false;
        if (std::fabs(new_atk - active_patch_.amp_attack) > eps) { active_patch_.amp_attack = new_atk; env_changed = true; }
        if (std::fabs(new_dec - active_patch_.amp_decay) > eps) { active_patch_.amp_decay = new_dec; env_changed = true; }
        if (std::fabs(new_sus - active_patch_.amp_sustain) > eps) { active_patch_.amp_sustain = new_sus; env_changed = true; }
        if (std::fabs(new_rel - active_patch_.amp_release) > eps) { active_patch_.amp_release = new_rel; env_changed = true; }
        if (env_changed) {
            amy_adapter_->setEnvelope(1, active_patch_.amp_attack, active_patch_.amp_decay,
                                      active_patch_.amp_sustain, active_patch_.amp_release);
        }
    }

    // Subtractive oscillator detune: base+1 is an operator slot in FM voices.
    if (supports_osc && std::fabs(new_detune - active_patch_.osc_detune) > eps) {
        active_patch_.osc_detune = new_detune;
        amy_adapter_->setOscDetune(1, new_detune);
    }

    // FX: independent commands, only when the value actually moved.
    if (std::fabs(new_chorus - fx_state_.chorus_depth) > eps) {
        fx_state_.chorus_depth = new_chorus;
        applyActiveChorusState();
    }
    if (std::fabs(new_reverb - fx_state_.reverb_mix) > eps) {
        fx_state_.reverb_mix = new_reverb;
        amy_adapter_->setReverb(fx_state_.reverb_size, 0.7f, new_reverb);
    }
    if (std::fabs(new_delay - fx_state_.delay_mix) > eps) {
        fx_state_.delay_mix = new_delay;
        amy_adapter_->setDelay(fx_state_.delay_time_ms, fx_state_.delay_feedback, new_delay);
    }
    if (std::fabs(new_drive - fx_state_.drive) > eps) {
        fx_state_.drive = new_drive;
        active_patch_.drive_level = new_drive;
        amy_adapter_->setDrive(new_drive);
    }
    if (std::fabs(new_tone - fx_state_.master_tone) > eps) {
        fx_state_.master_tone = new_tone;
        active_patch_.master_tone = new_tone;
        amy_adapter_->setMasterTone(new_tone);
    }

    // FM relative controls. The engine receives the composed final value once;
    // the manual base is never overwritten by the applied result.
    if (is_fm) {
        if (std::fabs(new_fm_mod - fm_state_.mod_factor) > eps) {
            fm_state_.mod_factor = new_fm_mod;
            amy_adapter_->setFmModIndex(1, new_fm_mod);
        }
        if (std::fabs(new_fm_ratio - fm_state_.ratio_factor) > eps) {
            fm_state_.ratio_factor = new_fm_ratio;
            amy_adapter_->setFmRatio(1, new_fm_ratio);
        }
        if (std::fabs(new_fm_detune - fm_state_.detune_cents) > eps) {
            fm_state_.detune_cents = new_fm_detune;
            amy_adapter_->setOscDetune(1, new_fm_detune);
        }
        if (std::fabs(new_fm_fb - fm_state_.feedback) > eps) {
            fm_state_.feedback = new_fm_fb;
            amy_adapter_->setFmFeedback(1, new_fm_fb);
        }
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
                if (activeFamily() == PatchFamily::FM) {
                    // FM mode: the same knobs are relative FM controls. The
                    // soft-takeover saved position must follow the *manual* base,
                    // not the macro-composed final state, so the knob can always
                    // reach the manual value it represents.
                    syncFmStateFromBaseline();
                    switch (knob_idx) {
                        case 0: saved_val = fmModNormFromFactor(manual_state_.fm_mod_factor) * 127.0f; break;
                        case 1: saved_val = fmRatioNormFromFactor(manual_state_.fm_ratio_factor) * 127.0f; break;
                        case 2: saved_val = fmDetuneNormFromCents(manual_state_.fm_detune_cents) * 127.0f; break;
                        case 3: saved_val = fmFreqMultNormFromMult(manual_state_.fm_freq_mult) * 127.0f; break;
                        case 4: break; // FM Mod Decay [N/A]
                        case 5: saved_val = fmFeedbackNormFromValue(manual_state_.fm_feedback) * 127.0f; break;
                        case 6: break; // FM Vibrato [N/A]
                        case 7: saved_val = fmAlgorithmNormFromValue(manual_state_.fm_algorithm) * 127.0f; break;
                        default: break;
                    }
                } else if (knob_idx == 0) saved_val = active_patch_.osc_mix * 127.0f;
                else if (knob_idx == 1) saved_val = (active_patch_.wave_type / 8.0f) * 127.0f;
                else if (knob_idx == 2) saved_val = std::clamp((manual_state_.osc_detune + 100.0f) / 200.0f * 127.0f, 0.0f, 127.0f);
                else if (knob_idx == 3) saved_val = std::clamp(((active_patch_.transpose / 12.0f + 2.0f) / 4.0f) * 127.0f, 0.0f, 127.0f);
                else if (knob_idx == 4) saved_val = active_patch_.sub_level * 127.0f;
                else if (knob_idx == 5) saved_val = active_patch_.noise_level * 127.0f;
                break;
            case KnobBank::BankC_FilterEnv:
                switch (knob_idx) {
                    case 0: saved_val = std::clamp(cutoffToNorm(manual_state_.filter_cutoff) * 127.0f, 0.0f, 127.0f); break;
                    case 1: saved_val = std::clamp(resonanceToNorm(manual_state_.filter_res) * 127.0f, 0.0f, 127.0f); break;
                    case 2: saved_val = std::clamp((manual_state_.filter_env + 4.0f) / 8.0f * 127.0f, 0.0f, 127.0f); break;
                    case 3: saved_val = std::clamp(envelopeMsToNorm(manual_state_.amp_attack) * 127.0f, 0.0f, 127.0f); break;
                    case 4: saved_val = std::clamp(envelopeMsToNorm(manual_state_.amp_decay) * 127.0f, 0.0f, 127.0f); break;
                    case 5: saved_val = std::clamp(manual_state_.amp_sustain * 127.0f, 0.0f, 127.0f); break;
                    case 6: saved_val = std::clamp(envelopeMsToNorm(manual_state_.amp_release) * 127.0f, 0.0f, 127.0f); break;
                    case 7: saved_val = std::clamp(active_filter_key_track_ / 2.0f * 127.0f, 0.0f, 127.0f); break;
                }
                break;
            case KnobBank::BankD_Effects:
                switch (knob_idx) {
                    case 0: saved_val = std::clamp(fx_state_.chorus_mode / 5.0f * 127.0f, 0.0f, 127.0f); break;
                    case 1: saved_val = std::clamp((manual_state_.delay_time_ms - 10.0f) / 990.0f * 127.0f, 0.0f, 127.0f); break;
                    case 2: saved_val = std::clamp(manual_state_.delay_feedback / 0.95f * 127.0f, 0.0f, 127.0f); break;
                    case 3: saved_val = std::clamp(wetToNorm(manual_state_.delay_mix) * 127.0f, 0.0f, 127.0f); break;
                    case 4: saved_val = std::clamp(manual_state_.reverb_size * 127.0f, 0.0f, 127.0f); break;
                    case 5: saved_val = std::clamp(wetToNorm(manual_state_.reverb_mix) * 127.0f, 0.0f, 127.0f); break;
                    case 6: saved_val = std::clamp(driveToNorm(manual_state_.drive) * 127.0f, 0.0f, 127.0f); break;
                    case 7: saved_val = std::clamp((manual_state_.master_tone + 1.0f) / 2.0f * 127.0f, 0.0f, 127.0f); break;
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
            if (activeFamily() == PatchFamily::FM) {
                bank_label = "BANK B: FM OPERATORS";
                switch (knob_idx) {
                    case 0: { // FM Mod Index
                        param_name = "FM MOD INDEX";
                        syncFmStateFromBaseline();
                        // Manual modulation factor: center (norm 0.5) = 1.0x,
                        // min = 0.0x, max = 4.0x. The macro multiplies this.
                        manual_state_.fm_mod_factor = fmModFactorFromNorm(norm_val);
                        recomputeMacroTargets();
                        break;
                    }
                    case 1: { // FM Operator Ratio (continuous relative fine ratio)
                        param_name = "FM OP RATIO";
                        syncFmStateFromBaseline();
                        // Manual ratio factor: -1 -> 0.5x, 0 -> 1.0x, +1 -> 2.0x.
                        manual_state_.fm_ratio_factor = fmRatioFactorFromNorm(norm_val);
                        recomputeMacroTargets();
                        break;
                    }
                    case 2: { // FM Detune
                        param_name = "FM DETUNE";
                        syncFmStateFromBaseline();
                        // Detune in cents: -25 .. 0 .. +25 (0 at center).
                        manual_state_.fm_detune_cents = fmDetuneCentsFromNorm(norm_val);
                        recomputeMacroTargets();
                        break;
                    }
                    case 3: { // FM Freq Multiplier
                        param_name = "FM FREQ MULT";
                        syncFmStateFromBaseline();
                        manual_state_.fm_freq_mult = fmFreqMultFromNorm(norm_val);
                        fm_state_.freq_mult = manual_state_.fm_freq_mult;
                        recomputeMacroTargets();
                        break;
                    }
                    case 4: { // FM Mod Decay
                        param_name = "FM DECAY [N/A]";
                        // Operator-level envelope modulation not yet wired; do not corrupt global amp envelope
                        break;
                    }
                    case 5: { // FM Feedback
                        param_name = "FM FEEDBACK";
                        syncFmStateFromBaseline();
                        manual_state_.fm_feedback = fmFeedbackFromNorm(norm_val);
                        recomputeMacroTargets();
                        break;
                    }
                    case 6: { // FM Vibrato
                        param_name = "FM VIB [N/A]";
                        // True pitch LFO vibrato not yet wired; do not apply fake chorus
                        break;
                    }
                    case 7: { // DX7 Algorithm (1 .. 32)
                        param_name = "DX7 ALGO";
                        uint8_t algo = fmAlgorithmFromNorm(norm_val);
                        manual_state_.fm_algorithm = algo;
                        fm_state_.algorithm = algo;
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
                        manual_state_.osc_detune = (norm_val - 0.5f) * 100.0f;
                        recomputeMacroTargets();
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
                    manual_state_.filter_cutoff = cutoffFromNorm(norm_val);
                    recomputeMacroTargets();
                    break;
                case 1: // Resonance
                    param_name = "RESONANCE";
                    manual_state_.filter_res = resonanceFromNorm(norm_val);
                    recomputeMacroTargets();
                    break;
                case 2: // ENV AMOUNT
                    param_name = "ENV AMOUNT";
                    manual_state_.filter_env = (norm_val - 0.5f) * 8.0f;
                    recomputeMacroTargets();
                    break;
                case 3: // Amp Attack
                    if (activeFamily() == PatchFamily::FM) { param_name = "AMP ATTACK [N/A]"; break; }
                    param_name = "AMP ATTACK";
                    manual_state_.amp_attack = envelopeMsFromNorm(norm_val);
                    recomputeMacroTargets();
                    break;
                case 4: // Amp Decay
                    if (activeFamily() == PatchFamily::FM) { param_name = "AMP DECAY [N/A]"; break; }
                    param_name = "AMP DECAY";
                    manual_state_.amp_decay = envelopeMsFromNorm(norm_val);
                    recomputeMacroTargets();
                    break;
                case 5: // Amp Sustain
                    if (activeFamily() == PatchFamily::FM) { param_name = "AMP SUSTAIN [N/A]"; break; }
                    param_name = "AMP SUSTAIN";
                    manual_state_.amp_sustain = norm_val;
                    recomputeMacroTargets();
                    break;
                case 6: // Amp Release
                    if (activeFamily() == PatchFamily::FM) { param_name = "AMP RELEASE [N/A]"; break; }
                    param_name = "AMP RELEASE";
                    manual_state_.amp_release = envelopeMsFromNorm(norm_val);
                    recomputeMacroTargets();
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
                    uint8_t old_mode = fx_state_.chorus_mode;
                    if (old_mode == 0 && mode != 0 && manual_state_.chorus_depth <= 0.001f) {
                        manual_state_.chorus_depth = 1.0f;
                    }
                    fx_state_.chorus_mode = mode;
                    active_patch_.chorus_mode = mode;
                    recomputeMacroTargets();
                    // The mode itself is not a macro target, so re-send the
                    // chorus configuration for the new mode even when depth and
                    // macro positions did not change.
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
                    manual_state_.delay_time_ms = delay_ms;
                    fx_state_.delay_time_ms = delay_ms;
                    if (amy_adapter_) amy_adapter_->setDelay(fx_state_.delay_time_ms, fx_state_.delay_feedback, fx_state_.delay_mix);
                    break;
                }
                case 2:
                    param_name = "DELAY FEEDBACK";
                    manual_state_.delay_feedback = norm_val * 0.95f;
                    fx_state_.delay_feedback = manual_state_.delay_feedback;
                    if (amy_adapter_) amy_adapter_->setDelay(fx_state_.delay_time_ms, fx_state_.delay_feedback, fx_state_.delay_mix);
                    break;
                case 3:
                    param_name = "DELAY MIX";
                    manual_state_.delay_mix = wetFromNorm(norm_val);
                    recomputeMacroTargets();
                    break;
                case 4:
                    param_name = "REVERB SIZE";
                    manual_state_.reverb_size = norm_val;
                    fx_state_.reverb_size = norm_val;
                    if (amy_adapter_) amy_adapter_->setReverb(fx_state_.reverb_size, 0.7f, fx_state_.reverb_mix);
                    break;
                case 5:
                    param_name = "REVERB MIX";
                    manual_state_.reverb_mix = wetFromNorm(norm_val);
                    recomputeMacroTargets();
                    break;
                case 6:
                    param_name = "DRIVE LEVEL";
                    manual_state_.drive = driveFromNorm(norm_val);
                    recomputeMacroTargets();
                    break;
                case 7:
                    param_name = "MASTER TONE";
                    manual_state_.master_tone = (norm_val - 0.5f) * 2.0f;
                    recomputeMacroTargets();
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
        float saved_val = std::clamp(cutoffToNorm(manual_state_.filter_cutoff) * 127.0f, 0.0f, 127.0f);
        status = soft_takeover_.update(takeover_id, physical_val, saved_val, effective_val);
        float norm = std::clamp(effective_val / 127.0f, 0.0f, 1.0f);
        manual_state_.filter_cutoff = cutoffFromNorm(norm);
        recomputeMacroTargets();
        break;
    }
    case 1: { // Knob B2: Resonance (0.5 .. 10.0)
        param_name = "RESONANCE";
        float saved_val = std::clamp(resonanceToNorm(manual_state_.filter_res) * 127.0f, 0.0f, 127.0f);
        status = soft_takeover_.update(takeover_id, physical_val, saved_val, effective_val);
        float norm = std::clamp(effective_val / 127.0f, 0.0f, 1.0f);
        manual_state_.filter_res = resonanceFromNorm(norm);
        recomputeMacroTargets();
        break;
    }
    case 2: { // Knob B3: Amp Attack (1ms .. 5000ms)
        param_name = "AMP ATTACK";
        float saved_val = std::clamp(envelopeMsToNorm(manual_state_.amp_attack) * 127.0f, 0.0f, 127.0f);
        status = soft_takeover_.update(takeover_id, physical_val, saved_val, effective_val);
        float norm = std::clamp(effective_val / 127.0f, 0.0f, 1.0f);
        if (activeFamily() == PatchFamily::FM) { param_name = "AMP ATTACK [N/A]"; break; }
        manual_state_.amp_attack = envelopeMsFromNorm(norm);
        recomputeMacroTargets();
        break;
    }
    case 3: { // Knob B4: Amp Release (1ms .. 5000ms)
        param_name = "AMP RELEASE";
        float saved_val = std::clamp(envelopeMsToNorm(manual_state_.amp_release) * 127.0f, 0.0f, 127.0f);
        status = soft_takeover_.update(takeover_id, physical_val, saved_val, effective_val);
        float norm = std::clamp(effective_val / 127.0f, 0.0f, 1.0f);
        if (activeFamily() == PatchFamily::FM) { param_name = "AMP RELEASE [N/A]"; break; }
        manual_state_.amp_release = envelopeMsFromNorm(norm);
        recomputeMacroTargets();
        break;
    }
    case 4: { // Knob B5: Chorus Depth (0% .. 100%)
        param_name = "CHORUS DEPTH";
        float saved_val = std::clamp(wetToNorm(manual_state_.chorus_depth) * 127.0f, 0.0f, 127.0f);
        status = soft_takeover_.update(takeover_id, physical_val, saved_val, effective_val);
        float norm = std::clamp(effective_val / 127.0f, 0.0f, 1.0f);
        manual_state_.chorus_depth = wetFromNorm(norm);
        recomputeMacroTargets();
        break;
    }
    case 5: { // Knob B6: Delay Time (10ms .. 1000ms)
        param_name = "DELAY TIME";
        float saved_val = std::clamp(delayMsToNorm(manual_state_.delay_time_ms) * 127.0f, 0.0f, 127.0f);
        status = soft_takeover_.update(takeover_id, physical_val, saved_val, effective_val);
        float norm = std::clamp(effective_val / 127.0f, 0.0f, 1.0f);
        manual_state_.delay_time_ms = delayMsFromNorm(norm);
        fx_state_.delay_time_ms = manual_state_.delay_time_ms;
        if (amy_adapter_) amy_adapter_->setDelay(fx_state_.delay_time_ms, fx_state_.delay_feedback, fx_state_.delay_mix);
        break;
    }
    case 6: { // Knob B7: Reverb Mix (0% .. 100%)
        param_name = "REVERB MIX";
        float saved_val = std::clamp(wetToNorm(manual_state_.reverb_mix) * 127.0f, 0.0f, 127.0f);
        status = soft_takeover_.update(takeover_id, physical_val, saved_val, effective_val);
        float norm = std::clamp(effective_val / 127.0f, 0.0f, 1.0f);
        manual_state_.reverb_mix = wetFromNorm(norm);
        recomputeMacroTargets();
        break;
    }
    case 7: { // Knob B8: Master Tone / FM Feedback / Drive
        const bool is_fm = (activeFamily() == PatchFamily::FM);
        param_name = is_fm ? "FM FEEDBACK" : "MASTER DRIVE";
        float saved_val;
        if (is_fm) {
            syncFmStateFromBaseline();
            saved_val = std::clamp(fmFeedbackNormFromValue(manual_state_.fm_feedback) * 127.0f, 0.0f, 127.0f);
        } else {
            saved_val = std::clamp(driveToNorm(manual_state_.drive) * 127.0f, 0.0f, 127.0f);
        }
        status = soft_takeover_.update(takeover_id, physical_val, saved_val, effective_val);
        float norm = std::clamp(effective_val / 127.0f, 0.0f, 1.0f);
        if (is_fm) {
            manual_state_.fm_feedback = fmFeedbackFromNorm(norm);
            recomputeMacroTargets();
        } else {
            manual_state_.drive = driveFromNorm(norm);
            recomputeMacroTargets();
        }
        break;
    }
    default:
        break;
}

if (ui_manager_) {
    uint8_t eng_vals[8] = {
        static_cast<uint8_t>(std::clamp(cutoffToNorm(active_patch_.filter_cutoff) * 127.0f, 0.0f, 127.0f)),
        static_cast<uint8_t>(std::clamp(resonanceToNorm(active_patch_.filter_res) * 127.0f, 0.0f, 127.0f)),
        static_cast<uint8_t>(std::clamp(envelopeMsToNorm(active_patch_.amp_attack) * 127.0f, 0.0f, 127.0f)),
        static_cast<uint8_t>(std::clamp(envelopeMsToNorm(active_patch_.amp_release) * 127.0f, 0.0f, 127.0f)),
        static_cast<uint8_t>(std::clamp(wetToNorm(fx_state_.chorus_depth) * 127.0f, 0.0f, 127.0f)),
        static_cast<uint8_t>(std::clamp(delayMsToNorm(fx_state_.delay_time_ms) * 127.0f, 0.0f, 127.0f)),
        static_cast<uint8_t>(std::clamp(wetToNorm(fx_state_.reverb_mix) * 127.0f, 0.0f, 127.0f)),
        static_cast<uint8_t>(std::clamp(driveToNorm(fx_state_.drive) * 127.0f, 0.0f, 127.0f))
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
    // Factory selection is provenance only. It deliberately leaves the selected
    // user storage slot untouched so browsing cannot retarget a user Save.
    active_patch_source_ = PatchSource::Factory;
    return applyLoadedPatch(*p);
}

void PatchManager::setActiveStorageSlot(uint8_t slot) {
    if (slot < StorageManager::kMaxSlots) {
        active_storage_slot_ = slot;
    } else {
        ESP_LOGW(TAG, "Ignoring out-of-range storage slot %u (max %u)",
                 slot, static_cast<unsigned>(StorageManager::kMaxSlots - 1));
    }
}

bool PatchManager::applyLoadedPatch(const SynthPatch& patch, uint8_t storage_slot) {
    // The storage location is runtime metadata, never folded into SynthPatch::id.
    // StorageManager::loadPatch has already range-checked the slot.
    active_storage_slot_ = storage_slot;
    active_patch_source_ = PatchSource::Storage;
    return applyLoadedPatch(patch);
}

bool PatchManager::saveActivePatch(StorageManager& storage, uint8_t slot) {
    const SynthPatch persisted = buildPersistablePatch();
    if (!storage.savePatch(slot, persisted)) {
        return false;
    }
    // A successful explicit save becomes the selected slot, so a following
    // unqualified Save (e.g. the long-hold gesture) repeats at the same place.
    active_storage_slot_ = slot;
    return true;
}

bool PatchManager::saveActivePatch(StorageManager& storage) {
    return saveActivePatch(storage, active_storage_slot_);
}

bool PatchManager::applyLoadedPatch(const SynthPatch& patch) {
    // Factory selection and stored-patch reload converge here. The caller owns
    // provenance and validation (factory table lookup, or StorageManager CRC
    // verification); this path only applies the patch consistently.
    active_patch_ = patch;
    active_patch_.crc32 = calculatePatchCrc32(active_patch_);

    ESP_LOGI(TAG, "Loaded Patch #%d [%s] (CRC32: 0x%08X)", 
             active_patch_.id, active_patch_.name, static_cast<unsigned int>(active_patch_.crc32));

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
        char m_labels[8][8];
        for (int i = 0; i < 8; ++i) {
            m_vals[i] = static_cast<uint8_t>(active_patch_.macros[i].current_val);
            snprintf(m_labels[i], sizeof(m_labels[i]), "%s", active_patch_.macros[i].name);
        }
        ui_manager_->homeScreen().setPatchInfo(active_patch_.id, active_patch_.name, "SYNTH");
        ui_manager_->homeScreen().setMacroLabels(m_labels);
        ui_manager_->homeScreen().setMacroValues(m_vals);
        ui_manager_->homeScreen().setActiveVoices(amy_adapter_ ? amy_adapter_->activeVoices() : 0, active_patch_.voice_count);
        ui_manager_->triggerParameterOverlay("PATCH LOAD", "BANK A", (float)active_patch_.id, 0.0f, active_patch_.name, TakeoverStatus::Captured);
    }

    return true;
}

SynthPatch PatchManager::buildPersistablePatch() const {
    SynthPatch out = active_patch_;

    // A persisted patch must represent MANUAL STATE + MACRO POSITIONS, never the
    // final macro-processed state plus macro positions, otherwise the macro is
    // applied twice on reload.
    //
    // Only the family-aware relative model has a distinct manual base. Legacy
    // absolute macros (0..7) write their final value directly into active_patch_
    // and are persisted as-is; the load path never re-runs them. None also needs
    // no substitution. Mixed applies the relative model, so it uses the manual
    // base like RelativeOnly.
    const MacroMappingMode mode = macroMappingMode();
    if (mode == MacroMappingMode::RelativeOnly || mode == MacroMappingMode::Mixed) {
        out.filter_cutoff     = manual_state_.filter_cutoff;
        out.filter_res        = manual_state_.filter_res;
        out.filter_env_amount = manual_state_.filter_env;

        out.amp_attack        = manual_state_.amp_attack;
        out.amp_decay         = manual_state_.amp_decay;
        out.amp_sustain       = manual_state_.amp_sustain;
        out.amp_release       = manual_state_.amp_release;

        out.osc_detune        = manual_state_.osc_detune;

        // Only FX fields that exist in the v5 format are persisted. Chorus
        // depth, delay time/feedback/mix, reverb size/mix and all Bank B FM
        // runtime edits remain runtime-only until a later format bump.
        out.drive_level       = manual_state_.drive;
        out.master_tone       = manual_state_.master_tone;
    }

    // Macro positions, defaults, mappings and names are carried unchanged from
    // active_patch_. The CRC is intentionally left unset: StorageManager is the
    // single place that stamps id + CRC on save.
    out.crc32 = 0;
    return out;
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

    const MacroMappingMode mapping_mode = macroMappingMode();
    if (mapping_mode == MacroMappingMode::Mixed) {
        // A patch mixing legacy and relative routes is unsupported. Apply the
        // relative model and say so once per load instead of silently dropping
        // the legacy routes.
        if (!mixed_macro_warning_emitted_) {
            ESP_LOGW(TAG, "Patch #%u mixes legacy and relative macro mappings; legacy routes are ignored",
                     active_patch_.id);
            mixed_macro_warning_emitted_ = true;
        }
    }

    if (mapping_mode == MacroMappingMode::RelativeOnly ||
        mapping_mode == MacroMappingMode::Mixed) {
        // Family-aware factory profiles: rebuild every target from the manual
        // state so results are deterministic and order-independent.
        recomputeMacroTargets();
    } else if (mapping_mode == MacroMappingMode::LegacyOnly) {
        // Legacy patches keep their original absolute macro behavior.
        applyMacroToEngine(macro_idx, effective_val);
    }

    if (ui_manager_) {
        uint8_t m_vals[8];
        for (int i = 0; i < 8; ++i) m_vals[i] = static_cast<uint8_t>(active_patch_.macros[i].current_val);
        ui_manager_->homeScreen().setMacroValues(m_vals);
        ui_manager_->triggerParameterOverlay(macro.name, "SYNTH", effective_val, macro.default_val, "", status);
    }
}

void PatchManager::applyPatchToEngine(const SynthPatch& patch) {
    if (!amy_adapter_) return;

    mixed_macro_warning_emitted_ = false;

    // A new preset establishes a new FM baseline. Drop the previous patch's
    // runtime FM controls so the centered positions reproduce the new timbre,
    // and invalidate the engine baseline until the preset materializes.
    fm_state_ = FmControlState{};
    amy_adapter_->invalidateFmBaseline();

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

    // 3. Configure Filter & Envelopes. AMY broadcasts synth-level amp/freq
    // coefficients to every osc of the voice; for an ALGO voice that includes
    // the DX7 operators, so a generic ADSR would overwrite their preset levels.
    // The FM preset owns its operator envelopes, so the generic amp envelope is
    // only sent to non-FM families.
    active_filter_env_amt_   = patch.filter_env_amount;
    active_filter_key_track_ = patch.filter_key_tracking;
    active_filter_vel_track_ = patch.filter_vel_tracking;
    active_filter_type_      = patch.filter_type;
    applyActiveFilterState();
    const PatchFamily family = classifyPatch(patch);
    if (supportsGenericAmpEnvelope(family)) {
        amy_adapter_->setEnvelope(1, patch.amp_attack, patch.amp_decay, patch.amp_sustain, patch.amp_release);
    }

    // 4. Configure Oscillator & FX state. These controls address base/base+1/
    // base+2/base+3 as a subtractive voice (main/sub/noise). In an ALGO
    // (DX7/FM) preset those positions are the FM control osc and its operators,
    // so applying them would overwrite operator levels/ratios and make the
    // played state diverge from the captured FM baseline. FM has its own
    // controls; skip the subtractive ones for FM.
    if (supportsSubtractiveOscControls(family)) {
        amy_adapter_->setOscMix(1, patch.osc_mix);
        amy_adapter_->setOscDetune(1, patch.osc_detune);
        amy_adapter_->setSubOscLevel(1, patch.sub_level);
        amy_adapter_->setNoiseLevel(1, patch.noise_level);
    }

    fx_state_.drive       = std::clamp(patch.drive_level, 0.0f, 1.0f);
    fx_state_.master_tone = patch.master_tone;

    uint8_t old_mode = fx_state_.chorus_mode;
    fx_state_.chorus_mode = patch.chorus_mode;
    if (old_mode == 0 && patch.chorus_mode != 0 && fx_state_.chorus_depth <= 0.001f) {
        fx_state_.chorus_depth = 1.0f;
    }

    amy_adapter_->setDrive(fx_state_.drive);
    amy_adapter_->setMasterTone(fx_state_.master_tone);
    applyActiveChorusState();
    amy_adapter_->setReverbFreeze(patch.reverb_freeze != 0);

    // 5. Set default reverb & delay effect levels
    amy_adapter_->setReverb(fx_state_.reverb_size, 0.7f, fx_state_.reverb_mix);
    amy_adapter_->setDelay(fx_state_.delay_time_ms, fx_state_.delay_feedback, fx_state_.delay_mix);

    // 5b. Capture the runtime manual-control state now that the patch and its FX
    // state are fully configured. Macros compose on top of this base; the patch
    // format itself is untouched.
    captureManualControlState();

    // 5c. Re-apply the macro composition for the relative model. A factory patch
    // loads with neutral macro positions so this is silent; a persisted
    // snapshot restored non-neutral positions and its sound parameters hold the
    // manual base, so recomposing reaches the same effective state as before
    // save. Legacy absolute macros are applied directly at setMacro() time and
    // must not be recomposed here.
    const MacroMappingMode mapping_mode = macroMappingMode();
    if (mapping_mode == MacroMappingMode::RelativeOnly ||
        mapping_mode == MacroMappingMode::Mixed) {
        recomputeMacroTargets();
    }

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
    const bool is_fm = (activeFamily() == PatchFamily::FM);

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
                if (!is_fm || active_patch_.engine_patch == 0) {
                    active_patch_.filter_cutoff = target_val;
                    applyActiveFilterState();
                }
                break;
            case 1: // Filter Res (Subtractive/Juno)
                if (!is_fm || active_patch_.engine_patch == 0) {
                    active_patch_.filter_res = target_val;
                    applyActiveFilterState();
                }
                break;
            case 2: // Brightness
                if (!is_fm) { // Subtractive / Juno
                    active_patch_.filter_cutoff = target_val;
                    applyActiveFilterState();
                } else {
                    float mod_factor = fmModFactorFromNorm(norm_val);
                    fm_state_.mod_factor = mod_factor;
                    amy_adapter_->setFmModIndex(1, mod_factor);
                }
                break;
            case 3: // Amp Attack
                // FM presets own their operator envelopes: the generic ADSR must
                // not touch amp_coefs or the DX7 operators.
                if (is_fm) break;
                active_patch_.amp_attack = target_val;
                amy_adapter_->setEnvelope(1, target_val, active_patch_.amp_decay, active_patch_.amp_sustain, active_patch_.amp_release);
                break;
            case 4: // Amp Release
                if (is_fm) break;
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
                if (is_fm) {
                    float safe_fb = std::clamp(shaped_val * 0.16f, 0.0f, 0.16f);
                    fm_state_.feedback = safe_fb;
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
