// Run with tests/run_smk_patch_integrity.ps1.
// Uses the real PatchManager on top of the real AmyAdapter and the real AMY
// engine, so it validates the actual operator state rather than a mock.
#include "patch_manager.h"
#include "factory_patches.h"
#include "amy_adapter.h"
#include "synth_config.h"
#include "diagnostics.h"
#include "ui_manager.h"
#include "clock_manager.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace smk {
Diagnostics& Diagnostics::instance() { static Diagnostics instance; return instance; }
DiagnosticCounters& Diagnostics::counters() { return counters_; }
struct AmyAdapterTestAccess {
    static bool service(AmyAdapter& adapter) { return adapter.serviceBlock(); }
};

// UI/clock stubs required to link PatchManager. The test never drives the UI.
void ClockManager::setBpm(float) {}
void HomeScreen::setPatchInfo(uint16_t, const char*, const char*) {}
void HomeScreen::setMacroValues(const uint8_t[8]) {}
void HomeScreen::setMacroLabels(const char[8][8]) {}
void HomeScreen::setEngineValues(const uint8_t[8]) {}
void HomeScreen::setHomeKnobBankView(HomeKnobBankView) {}
void HomeScreen::setKnobBankLabel(const char*) {}
void HomeScreen::setActiveVoices(uint8_t, uint8_t) {}
void UIManager::triggerParameterOverlay(const char*, const char*, float, float, const char*, TakeoverStatus) {}
}

extern "C" {
#include "amy.h"
void delay_ms(uint32_t) {}
void amy_update_tasks() {}
int16_t* amy_render_audio() { return amy_simple_fill_buffer(); }
size_t amy_i2s_write(const uint8_t*, size_t nbytes) { return nbytes; }
struct FmAlgorithm { uint8_t ops[MAX_ALGO_OPS]; };
extern const struct FmAlgorithm algorithms[33];
}

struct OpCapture {
    int16_t osc = -1;
    float amp = 0.0f;
    float logratio = 0.0f;
    float logfreq = 0.0f;
    bool present = false;
};

struct VoiceCapture {
    uint8_t algorithm = 0;
    float feedback = 0.0f;
    OpCapture ops[MAX_ALGO_OPS];
};

static void process(smk::AmyAdapter& adapter, int blocks) {
    for (int b = 0; b < blocks; ++b) {
        assert(smk::AmyAdapterTestAccess::service(adapter));
        adapter.render();
    }
}

static bool captureVoice0(VoiceCapture& out) {
    uint16_t voices[MAX_VOICES_PER_INSTRUMENT];
    const int nv = instrument_get_num_voices(1, voices);
    if (nv <= 0) return false;
    const uint16_t base = voice_to_base_osc[voices[0]];
    if (!AMY_IS_SET(base)) return false;
    out.algorithm = synth[base]->algorithm;
    out.feedback = synth[base]->feedback;
    for (int op = 0; op < MAX_ALGO_OPS; ++op) {
        const int16_t o = synth[base]->algo_source[op];
        out.ops[op].osc = o;
        out.ops[op].present = AMY_IS_SET(o);
        if (out.ops[op].present) {
            out.ops[op].amp = synth[o]->amp_coefs[COEF_CONST];
            out.ops[op].logratio = synth[o]->logratio;
            out.ops[op].logfreq = synth[o]->logfreq_coefs[COEF_CONST];
        }
    }
    return true;
}

int main() {
    // The factory DX7 patch carries subtractive defaults (osc_mix 0.5, detune 0,
    // sub 0, noise 0). These must not reach the FM operators.
    const smk::SynthPatch* p128 = smk::FactoryPatches::getPatchById(128);
    assert(p128->wave_type == smk::toAmyWaveType(smk::SmkWaveType::Algo));
    assert(std::fabs(p128->osc_mix - 0.5f) < 1e-6f);
    assert(std::fabs(p128->osc_detune - 0.0f) < 1e-6f);
    assert(std::fabs(p128->sub_level - 0.0f) < 1e-6f);
    assert(std::fabs(p128->noise_level - 0.0f) < 1e-6f);

    smk::AmyAdapter adapter;
    assert(adapter.begin(AMY_SAMPLE_RATE));

    // 1. Reference: the preset loaded directly through AmyAdapter.
    adapter.loadPreset(1, 128, p128->voice_count);
    process(adapter, 4);
    VoiceCapture reference;
    assert(captureVoice0(reference));
    assert(reference.algorithm >= 1 && reference.algorithm <= 32);
    for (int op = 0; op < MAX_ALGO_OPS; ++op) assert(reference.ops[op].present);

    // 2. Same patch through PatchManager (which also loads patch 0 in begin()).
    smk::PatchManager pm;
    assert(pm.begin(&adapter, nullptr));
    assert(pm.selectPatch(128));
    process(adapter, 6);
    VoiceCapture via_pm;
    assert(captureVoice0(via_pm));

    assert(via_pm.algorithm == reference.algorithm);
    for (int op = 0; op < MAX_ALGO_OPS; ++op) {
        assert(via_pm.ops[op].present == reference.ops[op].present);
        if (!reference.ops[op].present) continue;
        assert(via_pm.ops[op].osc == reference.ops[op].osc);
        assert(std::fabs(via_pm.ops[op].amp - reference.ops[op].amp) < 1e-5f);
        assert(std::fabs(via_pm.ops[op].logratio - reference.ops[op].logratio) < 1e-5f);
        assert(std::fabs(via_pm.ops[op].logfreq - reference.ops[op].logfreq) < 1e-5f);
    }

    // 3. Neutral FM runtime controls must reproduce the raw baseline exactly.
    adapter.setFmModIndex(1, 1.0f);
    adapter.setFmRatio(1, 1.0f);
    adapter.setOscDetune(1, 0.0f);
    process(adapter, 2);
    VoiceCapture neutral;
    assert(captureVoice0(neutral));
    for (int op = 0; op < MAX_ALGO_OPS; ++op) {
        if (!reference.ops[op].present) continue;
        assert(std::fabs(neutral.ops[op].amp - reference.ops[op].amp) < 1e-5f);
        assert(std::fabs(neutral.ops[op].logratio - reference.ops[op].logratio) < 1e-5f);
        assert(std::fabs(neutral.ops[op].logfreq - reference.ops[op].logfreq) < 1e-5f);
    }

    // 4. Live FM control integrity. Every generic amp ADSR control must leave
    // the DX7 operators exactly at the loaded baseline. Use Jump takeover so
    // each move would be applied immediately if the family guard were missing.
    const float atk_before = pm.activePatch().amp_attack;
    const float dec_before = pm.activePatch().amp_decay;
    const float sus_before = pm.activePatch().amp_sustain;
    const float rel_before = pm.activePatch().amp_release;

    process(adapter, 2);
    VoiceCapture baseline;
    assert(captureVoice0(baseline));

    auto assert_operators_match = [&](const char* label) {
        process(adapter, 2);
        VoiceCapture now;
        assert(captureVoice0(now));
        if (now.algorithm != baseline.algorithm) {
            std::printf("FAIL: algorithm changed after %s\n", label);
            assert(false);
        }
        for (int op = 0; op < MAX_ALGO_OPS; ++op) {
            if (now.ops[op].present != baseline.ops[op].present ||
                now.ops[op].osc != baseline.ops[op].osc) {
                std::printf("FAIL: algo_source changed after %s (op %d)\n", label, op);
                assert(false);
            }
            if (!baseline.ops[op].present) continue;
            if (std::fabs(now.ops[op].amp - baseline.ops[op].amp) > 1e-5f ||
                std::fabs(now.ops[op].logratio - baseline.ops[op].logratio) > 1e-5f ||
                std::fabs(now.ops[op].logfreq - baseline.ops[op].logfreq) > 1e-5f) {
                std::printf("FAIL: operator %d changed after %s (amp %.5f->%.5f lr %.5f->%.5f lf %.5f->%.5f)\n",
                            op, label,
                            baseline.ops[op].amp, now.ops[op].amp,
                            baseline.ops[op].logratio, now.ops[op].logratio,
                            baseline.ops[op].logfreq, now.ops[op].logfreq);
                assert(false);
            }
        }
    };

    pm.softTakeover().setMode(smk::TakeoverMode::Jump);

    // Bank C generic ADSR (knob indices 3..6)
    pm.setKnobBank(smk::KnobBank::BankC_FilterEnv);
    pm.handleKnobInput(3, 0.0f);   assert_operators_match("Bank C Attack min");
    pm.handleKnobInput(3, 127.0f); assert_operators_match("Bank C Attack max");
    pm.handleKnobInput(4, 127.0f); assert_operators_match("Bank C Decay");
    pm.handleKnobInput(5, 0.0f);   assert_operators_match("Bank C Sustain min");
    pm.handleKnobInput(5, 127.0f); assert_operators_match("Bank C Sustain max");
    pm.handleKnobInput(6, 127.0f); assert_operators_match("Bank C Release");

    // Engine generic ADSR (b_idx 2/3 -> knob indices 10/11)
    pm.handleKnobInput(10, 127.0f); assert_operators_match("Engine Attack");
    pm.handleKnobInput(11, 0.0f);   assert_operators_match("Engine Release");

    assert(pm.activePatch().amp_attack == atk_before);
    assert(pm.activePatch().amp_decay == dec_before);
    assert(pm.activePatch().amp_sustain == sus_before);
    assert(pm.activePatch().amp_release == rel_before);

    // Sound & Musicality M2: the FM macro profile is CHAR / BRTE / MOTN /
    // FDBK / RATIO / DTUNE / SPCE / EDGE. Ratio, detune and feedback are
    // allowed to change the operators; the algorithm and routing must never
    // change, and neutral must restore the captured baseline exactly.
    assert(std::strcmp(pm.activePatch().macros[3].name, "FDBK") == 0);
    assert(std::strcmp(pm.activePatch().macros[4].name, "RATIO") == 0);
    assert(std::strcmp(pm.activePatch().macros[5].name, "DTUNE") == 0);
    assert(std::strcmp(pm.activePatch().macros[6].name, "SPCE") == 0);
    assert(std::strcmp(pm.activePatch().macros[7].name, "EDGE") == 0);

    // The control state is pulled from the engine on the first FM interaction,
    // so compare against the engine-captured baseline algorithm.
    const uint8_t algo_baseline = baseline.algorithm;

    auto assert_routing_unchanged = [&](const char* label) {
        process(adapter, 2);
        VoiceCapture now;
        assert(captureVoice0(now));
        if (now.algorithm != baseline.algorithm) {
            std::printf("FAIL: FM algorithm changed after %s\n", label);
            assert(false);
        }
        for (int op = 0; op < MAX_ALGO_OPS; ++op) {
            if (now.ops[op].present != baseline.ops[op].present ||
                now.ops[op].osc != baseline.ops[op].osc) {
                std::printf("FAIL: FM routing changed after %s (op %d)\n", label, op);
                assert(false);
            }
        }
        if (pm.fmControlState().algorithm != algo_baseline) {
            std::printf("FAIL: FM algorithm control changed after %s\n", label);
            assert(false);
        }
    };

    const float fm_vals[] = { 0.0f, 127.0f };
    for (uint8_t m = 0; m < 8; ++m) {
        for (float v : fm_vals) {
            pm.setMacro(m, v, true);
            assert_routing_unchanged("FM macro extreme");
        }
        pm.setMacro(m, pm.activePatch().macros[m].default_val, true);
        assert_routing_unchanged("FM macro neutral");
    }

    // Every operator must be back at the captured baseline after neutral.
    assert_operators_match("FM all macros neutral");

    {
        process(adapter, 2);
        VoiceCapture restored;
        assert(captureVoice0(restored));
        if (std::fabs(restored.feedback - baseline.feedback) > 1e-6f) {
            std::printf("FAIL: FM feedback not restored (%.6f vs %.6f)\n",
                        restored.feedback, baseline.feedback);
            assert(false);
        }
    }

    // Sanity: the FDBK macro controls live engine feedback, and neutral restores it.
    {
        process(adapter, 2);
        VoiceCapture before;
        assert(captureVoice0(before));

        pm.setMacro(3, 127.0f, true);
        process(adapter, 2);
        VoiceCapture raised;
        assert(captureVoice0(raised));
        assert(raised.feedback >= before.feedback - 1e-6f);
        if (before.feedback < 0.16f - 1e-6f) {
            assert(std::fabs(raised.feedback - before.feedback) > 1e-6f);
        }

        pm.setMacro(3, pm.activePatch().macros[3].default_val, true);
        process(adapter, 2);
        VoiceCapture back;
        assert(captureVoice0(back));
        assert(std::fabs(back.feedback - before.feedback) < 1e-6f);
    }

    // ─────────────────────────────────────────────────────────────
    // M2.1: Bank B manual FM controls compose with the FM macros, and returning
    // each macro to neutral restores the *manual* state, not the preset one.
    // ─────────────────────────────────────────────────────────────
    pm.setKnobBank(smk::KnobBank::BankB_Oscillator);
    pm.softTakeover().setMode(smk::TakeoverMode::Jump);

    pm.handleKnobInput(0, smk::fmModNormFromFactor(2.0f) * 127.0f);   // mod index 2x
    pm.handleKnobInput(1, smk::fmRatioNormFromFactor(1.5f) * 127.0f); // ratio 1.5x
    pm.handleKnobInput(3, smk::fmFreqMultNormFromMult(3.0f) * 127.0f); // freq mult 3x
    pm.handleKnobInput(2, smk::fmDetuneNormFromCents(8.0f) * 127.0f);  // +8 cents
    pm.handleKnobInput(5, smk::fmFeedbackNormFromValue(0.08f) * 127.0f); // 0.08

    process(adapter, 2);
    VoiceCapture manual_fm;
    assert(captureVoice0(manual_fm));
    const float manual_feedback = manual_fm.feedback;
    assert(std::fabs(manual_feedback - 0.08f) < 1e-4f);
    assert(std::fabs(pm.fmControlState().mod_factor - 2.0f) < 1e-3f);
    assert(std::fabs(pm.fmControlState().ratio_factor - 4.5f) < 1e-3f); // 1.5 * 3
    assert(std::fabs(pm.fmControlState().detune_cents - 8.0f) < 1e-3f);

    auto captures_match = [&](const VoiceCapture& a, const VoiceCapture& b, const char* label) {
        if (a.algorithm != b.algorithm) {
            std::printf("FAIL: algorithm mismatch after %s\n", label);
            assert(false);
        }
        for (int op = 0; op < MAX_ALGO_OPS; ++op) {
            if (a.ops[op].present != b.ops[op].present || a.ops[op].osc != b.ops[op].osc) {
                std::printf("FAIL: routing mismatch after %s (op %d)\n", label, op);
                assert(false);
            }
            if (!a.ops[op].present) continue;
            if (std::fabs(a.ops[op].amp - b.ops[op].amp) > 1e-5f ||
                std::fabs(a.ops[op].logratio - b.ops[op].logratio) > 1e-5f ||
                std::fabs(a.ops[op].logfreq - b.ops[op].logfreq) > 1e-5f) {
                std::printf("FAIL: operator %d mismatch after %s\n", op, label);
                assert(false);
            }
        }
    };

    const uint8_t fm_macro_ids[] = { 0, 4, 5, 3 }; // CHAR, RATIO, DTUNE, FDBK
    for (uint8_t m : fm_macro_ids) {
        pm.setMacro(m, 127.0f, true);
        assert_routing_unchanged("FM manual+macro extreme");
        if (m == 4) {
            // manual ratio 1.5 * freq mult 3 * macro ratio 2 == 9x.
            assert(std::fabs(pm.fmControlState().ratio_factor - 9.0f) < 1e-3f);
        }
        process(adapter, 2);
        VoiceCapture changed;
        assert(captureVoice0(changed));
        if (m == 3) {
            // The FDBK macro must move live engine feedback away from manual.
            assert(std::fabs(changed.feedback - manual_feedback) > 1e-6f);
        }

        pm.setMacro(m, pm.activePatch().macros[m].default_val, true);
        assert_routing_unchanged("FM manual+macro neutral");
        process(adapter, 2);
        VoiceCapture restored;
        assert(captureVoice0(restored));
        captures_match(restored, manual_fm, "FM manual restore");
    }

    assert(std::fabs(pm.fmControlState().mod_factor - 2.0f) < 1e-3f);
    assert(std::fabs(pm.fmControlState().ratio_factor - 4.5f) < 1e-3f);
    assert(std::fabs(pm.fmControlState().detune_cents - 8.0f) < 1e-3f);
    assert(std::fabs(pm.fmControlState().feedback - 0.08f) < 1e-3f);
    assert(pm.fmControlState().algorithm == algo_baseline);
    assert(std::fabs(manual_fm.feedback - 0.08f) < 1e-4f);

    std::printf("PASS: real AMY FM manual controls + macros compose and neutral restores the manual state\n");

    std::printf("PASS: FM macros scale the timbre; algorithm/routing never change\n");
    std::printf("PASS: FM neutral restores operator levels/ratios/freqs and feedback exactly\n");

    std::printf("DX7 algorithm=%u ops:", (unsigned)reference.algorithm);
    for (int op = 0; op < MAX_ALGO_OPS; ++op) {
        std::printf(" [%d amp=%.4f lr=%.4f lf=%.4f]", op, reference.ops[op].amp,
                    reference.ops[op].logratio, reference.ops[op].logfreq);
    }
    std::printf("\nPASS: DX7 via AmyAdapter == DX7 via PatchManager (operators untouched)\n");
    std::printf("PASS: live FM ADSR controls (Bank C, Engine) leave operators at baseline\n");
    return 0;
}
