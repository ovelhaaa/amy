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

    std::printf("DX7 algorithm=%u ops:", (unsigned)reference.algorithm);
    for (int op = 0; op < MAX_ALGO_OPS; ++op) {
        std::printf(" [%d amp=%.4f lr=%.4f lf=%.4f]", op, reference.ops[op].amp,
                    reference.ops[op].logratio, reference.ops[op].logfreq);
    }
    std::printf("\nPASS: DX7 via AmyAdapter == DX7 via PatchManager (operators untouched)\n");
    return 0;
}
