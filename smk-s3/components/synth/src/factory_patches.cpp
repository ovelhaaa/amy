#include "factory_patches.h"
#include <cstring>

namespace smk {

uint32_t calculatePatchCrc32(const SynthPatch& patch) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&patch);
    size_t len = sizeof(SynthPatch) - sizeof(uint32_t); // Exclude the crc32 field itself
    
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= p[i];
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

static MacroConfig makeMacro(const char* name, float val, uint8_t param_type, float min_val, float max_val) {
    MacroConfig m = {};
    strncpy(m.name, name, sizeof(m.name) - 1);
    m.default_val = val;
    m.current_val = val;
    m.mapping_count = 1;
    m.mappings[0] = { 0xFFFF, param_type, min_val, max_val, 0 };
    return m;
}

static LayerConfig makeDefaultLayer(uint16_t engine_patch = 0, int8_t transpose = 0) {
    LayerConfig l = {};
    l.engine_patch = engine_patch;
    l.transpose = transpose;
    l.volume = 100;
    l.pan = 0;
    l.low_key = 0;
    l.high_key = 127;
    l.low_velocity = 0;
    l.high_velocity = 127;
    l.max_voices = 8;
    l.midi_channel = 0xFF; // Any channel
    l.flags = 1; // Enabled
    return l;
}

static SynthPatch createPatch0() {
    SynthPatch p = {};
    p.id = 0;
    strncpy(p.name, "000 DEFAULT SAW", sizeof(p.name) - 1);
    strncpy(p.category, "SYNTH LEAD", sizeof(p.category) - 1);
    strncpy(p.author, "SMK-S3", sizeof(p.author) - 1);
    p.mode = SynthMode::Single;
    p.split_point = 60;
    p.layer_a = makeDefaultLayer(0, 0);
    p.layer_b = makeDefaultLayer(0, 0);
    p.voice_count = 8;
    p.wave_type = 1; // SAW_DOWN
    p.base_freq = 440.0f;
    p.filter_cutoff = 3200.0f;
    p.filter_res = 1.2f;
    p.amp_attack = 10.0f;
    p.amp_decay = 200.0f;
    p.amp_sustain = 0.8f;
    p.amp_release = 150.0f;
    p.macros[0] = makeMacro("CHAR", 50.0f, 0, 800.0f, 8000.0f);
    p.macros[1] = makeMacro("BRTE", 60.0f, 2, 0.0f, 4.0f);
    p.macros[2] = makeMacro("MOTN", 20.0f, 5, 0.5f, 10.0f);
    p.macros[3] = makeMacro("SHAP", 40.0f, 3, 5.0f, 500.0f);
    p.macros[4] = makeMacro("ATK",  10.0f, 3, 1.0f, 200.0f);
    p.macros[5] = makeMacro("REL",  50.0f, 4, 10.0f, 1000.0f);
    p.macros[6] = makeMacro("SPCE", 30.0f, 6, 0.0f, 1.0f);
    p.macros[7] = makeMacro("DRV",  15.0f, 7, 1.0f, 5.0f);
    p.crc32 = calculatePatchCrc32(p);
    return p;
}

static SynthPatch createPatch1() {
    SynthPatch p = {};
    p.id = 1;
    strncpy(p.name, "001 WARM PAD", sizeof(p.name) - 1);
    strncpy(p.category, "PAD", sizeof(p.category) - 1);
    strncpy(p.author, "SMK-S3", sizeof(p.author) - 1);
    p.mode = SynthMode::Layer;
    p.split_point = 60;
    p.layer_a = makeDefaultLayer(1, 0);
    p.layer_b = makeDefaultLayer(3, 12); // Layer B transposed +12 semitones (shimmer)
    p.voice_count = 6;
    p.wave_type = 3; // TRIANGLE
    p.base_freq = 440.0f;
    p.filter_cutoff = 1800.0f;
    p.filter_res = 0.8f;
    p.amp_attack = 350.0f;
    p.amp_decay = 800.0f;
    p.amp_sustain = 0.9f;
    p.amp_release = 1200.0f;
    p.macros[0] = makeMacro("CHAR", 40.0f, 0, 400.0f, 4000.0f);
    p.macros[1] = makeMacro("BRTE", 35.0f, 2, 0.0f, 3.0f);
    p.macros[2] = makeMacro("MOTN", 45.0f, 5, 0.2f, 4.0f);
    p.macros[3] = makeMacro("SHAP", 60.0f, 3, 100.0f, 1000.0f);
    p.macros[4] = makeMacro("ATK",  45.0f, 3, 50.0f, 2000.0f);
    p.macros[5] = makeMacro("REL",  70.0f, 4, 100.0f, 3000.0f);
    p.macros[6] = makeMacro("SPCE", 65.0f, 6, 0.0f, 1.0f);
    p.macros[7] = makeMacro("DRV",   0.0f, 7, 1.0f, 2.0f);
    p.crc32 = calculatePatchCrc32(p);
    return p;
}

static SynthPatch createPatch2() {
    SynthPatch p = {};
    p.id = 2;
    strncpy(p.name, "002 MONO BASS", sizeof(p.name) - 1);
    strncpy(p.category, "BASS", sizeof(p.category) - 1);
    strncpy(p.author, "SMK-S3", sizeof(p.author) - 1);
    p.mode = SynthMode::Split;
    p.split_point = 60;
    p.layer_a = makeDefaultLayer(4, -12);
    p.layer_a.high_key = 59; // Left hand bass up to B3
    p.layer_b = makeDefaultLayer(1, 0);
    p.layer_b.low_key = 60;  // Right hand lead from C4 up
    p.voice_count = 1;
    p.wave_type = 4; // SQUARE
    p.base_freq = 110.0f;
    p.filter_cutoff = 950.0f;
    p.filter_res = 3.5f;
    p.amp_attack = 2.0f;
    p.amp_decay = 180.0f;
    p.amp_sustain = 0.2f;
    p.amp_release = 80.0f;
    p.macros[0] = makeMacro("CHAR", 75.0f, 0, 200.0f, 3500.0f);
    p.macros[1] = makeMacro("BRTE", 80.0f, 2, 1.0f, 4.0f);
    p.macros[2] = makeMacro("MOTN", 10.0f, 5, 0.1f, 8.0f);
    p.macros[3] = makeMacro("SHAP", 25.0f, 3, 1.0f, 100.0f);
    p.macros[4] = makeMacro("ATK",   5.0f, 3, 1.0f, 50.0f);
    p.macros[5] = makeMacro("REL",  20.0f, 4, 10.0f, 300.0f);
    p.macros[6] = makeMacro("SPCE", 10.0f, 6, 0.0f, 0.5f);
    p.macros[7] = makeMacro("DRV",  50.0f, 7, 1.0f, 10.0f);
    p.crc32 = calculatePatchCrc32(p);
    return p;
}

static SynthPatch createPatch42() {
    SynthPatch p = {};
    p.id = 42;
    strncpy(p.name, "042 GLASS HORIZON", sizeof(p.name) - 1);
    strncpy(p.category, "AMBIENT", sizeof(p.category) - 1);
    strncpy(p.author, "SMK-S3", sizeof(p.author) - 1);
    p.mode = SynthMode::Layer;
    p.split_point = 60;
    p.layer_a = makeDefaultLayer(0, 0);
    p.layer_b = makeDefaultLayer(0, 7); // Layer B transposed +7 semitones (fifth)
    p.voice_count = 8;
    p.wave_type = 0; // SINE
    p.base_freq = 440.0f;
    p.filter_cutoff = 2400.0f;
    p.filter_res = 2.1f;
    p.amp_attack = 120.0f;
    p.amp_decay = 600.0f;
    p.amp_sustain = 0.7f;
    p.amp_release = 850.0f;
    p.macros[0] = makeMacro("CHAR", 62.0f, 0, 500.0f, 6000.0f);
    p.macros[1] = makeMacro("BRTE", 78.0f, 2, 0.0f, 5.0f);
    p.macros[2] = makeMacro("MOTN", 31.0f, 5, 0.1f, 6.0f);
    p.macros[3] = makeMacro("SHAP", 45.0f, 3, 10.0f, 800.0f);
    p.macros[4] = makeMacro("ATK",   8.0f, 3, 2.0f, 500.0f);
    p.macros[5] = makeMacro("REL",  67.0f, 4, 50.0f, 2000.0f);
    p.macros[6] = makeMacro("SPCE", 34.0f, 6, 0.0f, 1.0f);
    p.macros[7] = makeMacro("DRV",  22.0f, 7, 1.0f, 4.0f);
    p.crc32 = calculatePatchCrc32(p);
    return p;
}

static SynthPatch createPatch80() {
    SynthPatch p = {};
    p.id = 80;
    strncpy(p.name, "080 VINTAGE 808", sizeof(p.name) - 1);
    strncpy(p.category, "DRUMS", sizeof(p.category) - 1);
    strncpy(p.author, "SMK-S3", sizeof(p.author) - 1);
    p.mode = SynthMode::Multi;
    p.split_point = 60;
    p.layer_a = makeDefaultLayer(7, 0);
    p.layer_b = makeDefaultLayer(7, 0);
    p.voice_count = 8;
    p.wave_type = 7; // PCM
    p.base_freq = 100.0f;
    p.filter_cutoff = 4000.0f;
    p.filter_res = 1.0f;
    p.amp_attack = 1.0f;
    p.amp_decay = 250.0f;
    p.amp_sustain = 0.0f;
    p.amp_release = 100.0f;
    p.macros[0] = makeMacro("CHAR", 50.0f, 0, 1000.0f, 8000.0f);
    p.macros[1] = makeMacro("BRTE", 50.0f, 2, 0.0f, 7.0f);
    p.macros[2] = makeMacro("MOTN",  0.0f, 5, 0.0f, 1.0f);
    p.macros[3] = makeMacro("SHAP", 50.0f, 3, 1.0f, 100.0f);
    p.macros[4] = makeMacro("ATK",   0.0f, 3, 0.0f, 10.0f);
    p.macros[5] = makeMacro("REL",  30.0f, 4, 10.0f, 500.0f);
    p.macros[6] = makeMacro("SPCE", 20.0f, 6, 0.0f, 0.8f);
    p.macros[7] = makeMacro("DRV",  40.0f, 7, 1.0f, 8.0f);
    p.crc32 = calculatePatchCrc32(p);
    return p;
}

static const SynthPatch s_factory_patches[FactoryPatches::kCount] = {
    createPatch0(),
    createPatch1(),
    createPatch2(),
    createPatch42(),
    createPatch80()
};

const SynthPatch* FactoryPatches::getPatchById(uint8_t patch_id) {
    for (size_t i = 0; i < kCount; ++i) {
        if (s_factory_patches[i].id == patch_id) {
            return &s_factory_patches[i];
        }
    }
    return &s_factory_patches[0]; // Default fallback
}

const SynthPatch* FactoryPatches::getPatchByIndex(size_t index) {
    if (index >= kCount) return &s_factory_patches[0];
    return &s_factory_patches[index];
}

} // namespace smk
