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

struct PatchDescriptor {
    uint8_t     id;
    const char* name;
    const char* category;
    const char* author;
    uint16_t    engine_patch;
    uint8_t     wave_type;    // 0=SINE, 1=SAW, 3=TRI, 4=SQR, 7=PCM, 8=ALGO/FM
    int8_t      transpose;
    uint8_t     voice_count;
    float       filter_cutoff;
    float       filter_res;
    float       amp_attack;
    float       amp_decay;
    float       amp_sustain;
    float       amp_release;
};

static const PatchDescriptor s_patch_table[FactoryPatches::kCount] = {
    // ═══════════════════════════════════════════════════════════════════════════════════════
    // Roland Juno-6 / Subtractive Analog Factory Library (IDs 0..127, Bank A & Bank B)
    // ═══════════════════════════════════════════════════════════════════════════════════════
    {   0, "000 A11 Brass Set 1    ", "BRASS         ", "ROLAND JUNO",   0, 1,   0, 10,  4000.0f,  1.0f,  25.0f,  400.0f, 0.80f,  250.0f },
    {   1, "001 A12 Brass Swell    ", "PAD           ", "ROLAND JUNO",   1, 3,   0, 10,  2500.0f,  0.8f, 350.0f,  800.0f, 0.90f, 1000.0f },
    {   2, "002 A13 Trumpet        ", "BRASS         ", "ROLAND JUNO",   2, 1,   0, 10,  4000.0f,  1.0f,  25.0f,  400.0f, 0.80f,  250.0f },
    {   3, "003 A14 Flutes         ", "WIND          ", "ROLAND JUNO",   3, 1,   0, 10,  4500.0f,  0.9f,  40.0f,  500.0f, 0.85f,  200.0f },
    {   4, "004 A15 Moving Strings ", "STRINGS       ", "ROLAND JUNO",   4, 1,   0, 10,  6000.0f,  0.9f, 180.0f,  800.0f, 0.85f,  600.0f },
    {   5, "005 A16 Brass & Strings", "STRINGS       ", "ROLAND JUNO",   5, 1,   0, 10,  6000.0f,  0.9f, 180.0f,  800.0f, 0.85f,  600.0f },
    {   6, "006 A17 Choir          ", "STRINGS       ", "ROLAND JUNO",   6, 1,   0, 10,  6000.0f,  0.9f, 180.0f,  800.0f, 0.85f,  600.0f },
    {   7, "007 A18 Piano I        ", "KEYS          ", "ROLAND JUNO",   7, 1,   0, 10,  8000.0f,  1.0f,   2.0f, 1200.0f, 0.30f,  250.0f },
    {   8, "008 A21 Organ I        ", "ORGAN         ", "ROLAND JUNO",   8, 1,   0, 10,  7000.0f,  1.0f,   5.0f,  200.0f, 1.00f,  100.0f },
    {   9, "009 A22 Organ II       ", "ORGAN         ", "ROLAND JUNO",   9, 1,   0, 10,  7000.0f,  1.0f,   5.0f,  200.0f, 1.00f,  100.0f },
    {  10, "010 A23 Combo Organ    ", "ORGAN         ", "ROLAND JUNO",  10, 1,   0, 10,  7000.0f,  1.0f,   5.0f,  200.0f, 1.00f,  100.0f },
    {  11, "011 A24 Calliope       ", "ORGAN         ", "ROLAND JUNO",  11, 1,   0, 10,  7000.0f,  1.0f,   5.0f,  200.0f, 1.00f,  100.0f },
    {  12, "012 A25 Donald Pluck   ", "PERC/BELL     ", "ROLAND JUNO",  12, 1,   0, 10,  9000.0f,  1.2f,   1.0f, 1000.0f, 0.10f,  400.0f },
    {  13, "013 A26 Celeste (1 oct.", "PERC/BELL     ", "ROLAND JUNO",  13, 1,   0, 10,  9000.0f,  1.2f,   1.0f, 1000.0f, 0.10f,  400.0f },
    {  14, "014 A27 Elect. Piano I ", "KEYS          ", "ROLAND JUNO",  14, 1,   0, 10,  8000.0f,  1.0f,   2.0f, 1200.0f, 0.30f,  250.0f },
    {  15, "015 A28 Elect. Piano II", "KEYS          ", "ROLAND JUNO",  15, 1,   0, 10,  8000.0f,  1.0f,   2.0f, 1200.0f, 0.30f,  250.0f },
    {  16, "016 A31 Clock Chimes (1", "PERC/BELL     ", "ROLAND JUNO",  16, 1,   0, 10,  9000.0f,  1.2f,   1.0f, 1000.0f, 0.10f,  400.0f },
    {  17, "017 A32 Steel Drums    ", "DRUMS/PERC    ", "ROLAND JUNO",  17, 7,   0,  4,  5000.0f,  1.0f,   1.0f,  350.0f, 0.00f,  150.0f },
    {  18, "018 A33 Xylophone      ", "PERC/BELL     ", "ROLAND JUNO",  18, 1,   0, 10,  9000.0f,  1.2f,   1.0f, 1000.0f, 0.10f,  400.0f },
    {  19, "019 A34 Brass III      ", "BRASS         ", "ROLAND JUNO",  19, 1,   0, 10,  4000.0f,  1.0f,  25.0f,  400.0f, 0.80f,  250.0f },
    {  20, "020 A35 Fanfare        ", "BRASS         ", "ROLAND JUNO",  20, 1,   0, 10,  4000.0f,  1.0f,  25.0f,  400.0f, 0.80f,  250.0f },
    {  21, "021 A36 String III     ", "STRINGS       ", "ROLAND JUNO",  21, 1,   0, 10,  6000.0f,  0.9f, 180.0f,  800.0f, 0.85f,  600.0f },
    {  22, "022 A37 Pizzicato      ", "SYNTH         ", "ROLAND JUNO",  22, 1,   0, 10,  3500.0f,  1.5f,  10.0f,  400.0f, 0.60f,  250.0f },
    {  23, "023 A38 High Strings   ", "STRINGS       ", "ROLAND JUNO",  23, 1,   0, 10,  6000.0f,  0.9f, 180.0f,  800.0f, 0.85f,  600.0f },
    {  24, "024 A41 Bass clarinet  ", "BASS          ", "ROLAND JUNO",  24, 4, -12,  4,  1200.0f,  2.5f,   5.0f,  250.0f, 0.20f,  100.0f },
    {  25, "025 A42 English Horn   ", "BRASS         ", "ROLAND JUNO",  25, 1,   0, 10,  4000.0f,  1.0f,  25.0f,  400.0f, 0.80f,  250.0f },
    {  26, "026 A43 Brass Ensemble ", "STRINGS       ", "ROLAND JUNO",  26, 1,   0, 10,  6000.0f,  0.9f, 180.0f,  800.0f, 0.85f,  600.0f },
    {  27, "027 A44 Guitar         ", "SYNTH         ", "ROLAND JUNO",  27, 1,   0, 10,  3500.0f,  1.5f,  10.0f,  400.0f, 0.60f,  250.0f },
    {  28, "028 A45 Koto           ", "PERC/BELL     ", "ROLAND JUNO",  28, 1,   0, 10,  9000.0f,  1.2f,   1.0f, 1000.0f, 0.10f,  400.0f },
    {  29, "029 A46 Dark Pluck     ", "PERC/BELL     ", "ROLAND JUNO",  29, 1,   0, 10,  9000.0f,  1.2f,   1.0f, 1000.0f, 0.10f,  400.0f },
    {  30, "030 A47 Funky I        ", "SYNTH         ", "ROLAND JUNO",  30, 1,   0, 10,  3500.0f,  1.5f,  10.0f,  400.0f, 0.60f,  250.0f },
    {  31, "031 A48 Synth Bass I (u", "BASS          ", "ROLAND JUNO",  31, 4, -12,  4,  1200.0f,  2.5f,   5.0f,  250.0f, 0.20f,  100.0f },
    {  32, "032 A51 Lead I         ", "SYNTH LEAD    ", "ROLAND JUNO",  32, 1,   0, 10,  5000.0f,  1.8f,   5.0f,  300.0f, 0.80f,  200.0f },
    {  33, "033 A52 Lead II        ", "SYNTH LEAD    ", "ROLAND JUNO",  33, 1,   0, 10,  5000.0f,  1.8f,   5.0f,  300.0f, 0.80f,  200.0f },
    {  34, "034 A53 Lead III       ", "SYNTH LEAD    ", "ROLAND JUNO",  34, 1,   0, 10,  5000.0f,  1.8f,   5.0f,  300.0f, 0.80f,  200.0f },
    {  35, "035 A54 Funky II       ", "SYNTH         ", "ROLAND JUNO",  35, 1,   0, 10,  3500.0f,  1.5f,  10.0f,  400.0f, 0.60f,  250.0f },
    {  36, "036 A55 Synth Bass II  ", "BASS          ", "ROLAND JUNO",  36, 4, -12,  4,  1200.0f,  2.5f,   5.0f,  250.0f, 0.20f,  100.0f },
    {  37, "037 A56 Funky III      ", "SYNTH         ", "ROLAND JUNO",  37, 1,   0, 10,  3500.0f,  1.5f,  10.0f,  400.0f, 0.60f,  250.0f },
    {  38, "038 A57 Thud Wah       ", "SYNTH FX      ", "ROLAND JUNO",  38, 1,   0,  8,  3000.0f,  2.0f, 100.0f, 1000.0f, 0.70f,  800.0f },
    {  39, "039 A58 Going Up       ", "SYNTH         ", "ROLAND JUNO",  39, 1,   0, 10,  3500.0f,  1.5f,  10.0f,  400.0f, 0.60f,  250.0f },
    {  40, "040 A61 Piano II       ", "KEYS          ", "ROLAND JUNO",  40, 1,   0, 10,  8000.0f,  1.0f,   2.0f, 1200.0f, 0.30f,  250.0f },
    {  41, "041 A62 Clav           ", "KEYS          ", "ROLAND JUNO",  41, 1,   0, 10,  8000.0f,  1.0f,   2.0f, 1200.0f, 0.30f,  250.0f },
    {  42, "042 A63 Frontier Organ ", "ORGAN         ", "ROLAND JUNO",  42, 1,   0, 10,  7000.0f,  1.0f,   5.0f,  200.0f, 1.00f,  100.0f },
    {  43, "043 A64 Snare Drum (uni", "DRUMS/PERC    ", "ROLAND JUNO",  43, 7,   0,  4,  5000.0f,  1.0f,   1.0f,  350.0f, 0.00f,  150.0f },
    {  44, "044 A65 Tom Toms (uniso", "DRUMS/PERC    ", "ROLAND JUNO",  44, 7,   0,  4,  5000.0f,  1.0f,   1.0f,  350.0f, 0.00f,  150.0f },
    {  45, "045 A66 Timpani (unison", "DRUMS/PERC    ", "ROLAND JUNO",  45, 7,   0,  4,  5000.0f,  1.0f,   1.0f,  350.0f, 0.00f,  150.0f },
    {  46, "046 A67 Shaker         ", "DRUMS/PERC    ", "ROLAND JUNO",  46, 7,   0,  4,  5000.0f,  1.0f,   1.0f,  350.0f, 0.00f,  150.0f },
    {  47, "047 A68 Synth Pad      ", "PAD           ", "ROLAND JUNO",  47, 3,   0, 10,  2500.0f,  0.8f, 350.0f,  800.0f, 0.90f, 1000.0f },
    {  48, "048 A71 Sweep I        ", "SYNTH FX      ", "ROLAND JUNO",  48, 1,   0,  8,  3000.0f,  2.0f, 100.0f, 1000.0f, 0.70f,  800.0f },
    {  49, "049 A72 Pluck Sweep    ", "PERC/BELL     ", "ROLAND JUNO",  49, 1,   0, 10,  9000.0f,  1.2f,   1.0f, 1000.0f, 0.10f,  400.0f },
    {  50, "050 A73 Repeater       ", "SYNTH FX      ", "ROLAND JUNO",  50, 1,   0,  8,  3000.0f,  2.0f, 100.0f, 1000.0f, 0.70f,  800.0f },
    {  51, "051 A74 Sweep II       ", "SYNTH FX      ", "ROLAND JUNO",  51, 1,   0,  8,  3000.0f,  2.0f, 100.0f, 1000.0f, 0.70f,  800.0f },
    {  52, "052 A75 Pluck Bell     ", "PERC/BELL     ", "ROLAND JUNO",  52, 1,   0, 10,  9000.0f,  1.2f,   1.0f, 1000.0f, 0.10f,  400.0f },
    {  53, "053 A76 Dark Synth Pian", "KEYS          ", "ROLAND JUNO",  53, 1,   0, 10,  8000.0f,  1.0f,   2.0f, 1200.0f, 0.30f,  250.0f },
    {  54, "054 A77 Sustainer      ", "SYNTH         ", "ROLAND JUNO",  54, 1,   0, 10,  3500.0f,  1.5f,  10.0f,  400.0f, 0.60f,  250.0f },
    {  55, "055 A78 Wah Release    ", "SYNTH FX      ", "ROLAND JUNO",  55, 1,   0,  8,  3000.0f,  2.0f, 100.0f, 1000.0f, 0.70f,  800.0f },
    {  56, "056 A81 Gong (play low ", "PERC/BELL     ", "ROLAND JUNO",  56, 1,   0, 10,  9000.0f,  1.2f,   1.0f, 1000.0f, 0.10f,  400.0f },
    {  57, "057 A82 Resonance Funk ", "SYNTH         ", "ROLAND JUNO",  57, 1,   0, 10,  3500.0f,  1.5f,  10.0f,  400.0f, 0.60f,  250.0f },
    {  58, "058 A83 Drum Booms (1 o", "DRUMS/PERC    ", "ROLAND JUNO",  58, 7,   0,  4,  5000.0f,  1.0f,   1.0f,  350.0f, 0.00f,  150.0f },
    {  59, "059 A84 Dust Storm     ", "SYNTH FX      ", "ROLAND JUNO",  59, 1,   0,  8,  3000.0f,  2.0f, 100.0f, 1000.0f, 0.70f,  800.0f },
    {  60, "060 A85 Rocket Men     ", "SYNTH FX      ", "ROLAND JUNO",  60, 1,   0,  8,  3000.0f,  2.0f, 100.0f, 1000.0f, 0.70f,  800.0f },
    {  61, "061 A86 Hand Claps     ", "DRUMS/PERC    ", "ROLAND JUNO",  61, 7,   0,  4,  5000.0f,  1.0f,   1.0f,  350.0f, 0.00f,  150.0f },
    {  62, "062 A87 FX Sweep       ", "SYNTH FX      ", "ROLAND JUNO",  62, 1,   0,  8,  3000.0f,  2.0f, 100.0f, 1000.0f, 0.70f,  800.0f },
    {  63, "063 A88 Caverns        ", "PAD           ", "ROLAND JUNO",  63, 3,   0, 10,  2500.0f,  0.8f, 350.0f,  800.0f, 0.90f, 1000.0f },
    {  64, "064 B11 Strings        ", "STRINGS       ", "ROLAND JUNO",  64, 1,   0, 10,  6000.0f,  0.9f, 180.0f,  800.0f, 0.85f,  600.0f },
    {  65, "065 B12 Violin         ", "STRINGS       ", "ROLAND JUNO",  65, 1,   0, 10,  6000.0f,  0.9f, 180.0f,  800.0f, 0.85f,  600.0f },
    {  66, "066 B13 Chorus Vibes   ", "PERC/BELL     ", "ROLAND JUNO",  66, 1,   0, 10,  9000.0f,  1.2f,   1.0f, 1000.0f, 0.10f,  400.0f },
    {  67, "067 B14 Organ 1        ", "ORGAN         ", "ROLAND JUNO",  67, 1,   0, 10,  7000.0f,  1.0f,   5.0f,  200.0f, 1.00f,  100.0f },
    {  68, "068 B15 Harpsichord 1  ", "KEYS          ", "ROLAND JUNO",  68, 1,   0, 10,  8000.0f,  1.0f,   2.0f, 1200.0f, 0.30f,  250.0f },
    {  69, "069 B16 Recorder       ", "WIND          ", "ROLAND JUNO",  69, 1,   0, 10,  4500.0f,  0.9f,  40.0f,  500.0f, 0.85f,  200.0f },
    {  70, "070 B17 Perc. Pluck    ", "PERC/BELL     ", "ROLAND JUNO",  70, 1,   0, 10,  9000.0f,  1.2f,   1.0f, 1000.0f, 0.10f,  400.0f },
    {  71, "071 B18 Noise Sweep    ", "SYNTH FX      ", "ROLAND JUNO",  71, 1,   0,  8,  3000.0f,  2.0f, 100.0f, 1000.0f, 0.70f,  800.0f },
    {  72, "072 B21 Space Chimes   ", "PERC/BELL     ", "ROLAND JUNO",  72, 1,   0, 10,  9000.0f,  1.2f,   1.0f, 1000.0f, 0.10f,  400.0f },
    {  73, "073 B22 Nylon Guitar   ", "SYNTH         ", "ROLAND JUNO",  73, 1,   0, 10,  3500.0f,  1.5f,  10.0f,  400.0f, 0.60f,  250.0f },
    {  74, "074 B23 Orchestral Pad ", "PAD           ", "ROLAND JUNO",  74, 3,   0, 10,  2500.0f,  0.8f, 350.0f,  800.0f, 0.90f, 1000.0f },
    {  75, "075 B24 Bright Pluck   ", "PERC/BELL     ", "ROLAND JUNO",  75, 1,   0, 10,  9000.0f,  1.2f,   1.0f, 1000.0f, 0.10f,  400.0f },
    {  76, "076 B25 Organ Bell     ", "ORGAN         ", "ROLAND JUNO",  76, 1,   0, 10,  7000.0f,  1.0f,   5.0f,  200.0f, 1.00f,  100.0f },
    {  77, "077 B26 Accordion      ", "KEYS          ", "ROLAND JUNO",  77, 1,   0, 10,  8000.0f,  1.0f,   2.0f, 1200.0f, 0.30f,  250.0f },
    {  78, "078 B27 FX Rise 1      ", "SYNTH FX      ", "ROLAND JUNO",  78, 1,   0,  8,  3000.0f,  2.0f, 100.0f, 1000.0f, 0.70f,  800.0f },
    {  79, "079 B28 FX Rise 2      ", "SYNTH FX      ", "ROLAND JUNO",  79, 1,   0,  8,  3000.0f,  2.0f, 100.0f, 1000.0f, 0.70f,  800.0f },
    {  80, "080 B31 Brass          ", "BRASS         ", "ROLAND JUNO",  80, 1,   0, 10,  4000.0f,  1.0f,  25.0f,  400.0f, 0.80f,  250.0f },
    {  81, "081 B32 Helicopter     ", "SYNTH FX      ", "ROLAND JUNO",  81, 1,   0,  8,  3000.0f,  2.0f, 100.0f, 1000.0f, 0.70f,  800.0f },
    {  82, "082 B33 Lute           ", "SYNTH         ", "ROLAND JUNO",  82, 1,   0, 10,  3500.0f,  1.5f,  10.0f,  400.0f, 0.60f,  250.0f },
    {  83, "083 B34 Chorus Funk    ", "SYNTH         ", "ROLAND JUNO",  83, 1,   0, 10,  3500.0f,  1.5f,  10.0f,  400.0f, 0.60f,  250.0f },
    {  84, "084 B35 Tomita         ", "DRUMS/PERC    ", "ROLAND JUNO",  84, 7,   0,  4,  5000.0f,  1.0f,   1.0f,  350.0f, 0.00f,  150.0f },
    {  85, "085 B36 FX Sweep 1     ", "SYNTH FX      ", "ROLAND JUNO",  85, 1,   0,  8,  3000.0f,  2.0f, 100.0f, 1000.0f, 0.70f,  800.0f },
    {  86, "086 B37 Sharp Reed     ", "WIND          ", "ROLAND JUNO",  86, 1,   0, 10,  4500.0f,  0.9f,  40.0f,  500.0f, 0.85f,  200.0f },
    {  87, "087 B38 Bass Pluck     ", "BASS          ", "ROLAND JUNO",  87, 4, -12,  4,  1200.0f,  2.5f,   5.0f,  250.0f, 0.20f,  100.0f },
    {  88, "088 B41 Resonant Rise  ", "SYNTH FX      ", "ROLAND JUNO",  88, 1,   0,  8,  3000.0f,  2.0f, 100.0f, 1000.0f, 0.70f,  800.0f },
    {  89, "089 B42 Harpsichord 2  ", "KEYS          ", "ROLAND JUNO",  89, 1,   0, 10,  8000.0f,  1.0f,   2.0f, 1200.0f, 0.30f,  250.0f },
    {  90, "090 B43 Dark Ensemble  ", "STRINGS       ", "ROLAND JUNO",  90, 1,   0, 10,  6000.0f,  0.9f, 180.0f,  800.0f, 0.85f,  600.0f },
    {  91, "091 B44 Contact Wah    ", "SYNTH FX      ", "ROLAND JUNO",  91, 1,   0,  8,  3000.0f,  2.0f, 100.0f, 1000.0f, 0.70f,  800.0f },
    {  92, "092 B45 Noise Sweep 2  ", "SYNTH FX      ", "ROLAND JUNO",  92, 1,   0,  8,  3000.0f,  2.0f, 100.0f, 1000.0f, 0.70f,  800.0f },
    {  93, "093 B46 Glassy Wah     ", "SYNTH FX      ", "ROLAND JUNO",  93, 1,   0,  8,  3000.0f,  2.0f, 100.0f, 1000.0f, 0.70f,  800.0f },
    {  94, "094 B47 Phase Ensemble ", "STRINGS       ", "ROLAND JUNO",  94, 1,   0, 10,  6000.0f,  0.9f, 180.0f,  800.0f, 0.85f,  600.0f },
    {  95, "095 B48 Chorused Bell  ", "PERC/BELL     ", "ROLAND JUNO",  95, 1,   0, 10,  9000.0f,  1.2f,   1.0f, 1000.0f, 0.10f,  400.0f },
    {  96, "096 B51 Clav           ", "KEYS          ", "ROLAND JUNO",  96, 1,   0, 10,  8000.0f,  1.0f,   2.0f, 1200.0f, 0.30f,  250.0f },
    {  97, "097 B52 Organ 2        ", "ORGAN         ", "ROLAND JUNO",  97, 1,   0, 10,  7000.0f,  1.0f,   5.0f,  200.0f, 1.00f,  100.0f },
    {  98, "098 B53 Bassoon        ", "BASS          ", "ROLAND JUNO",  98, 4, -12,  4,  1200.0f,  2.5f,   5.0f,  250.0f, 0.20f,  100.0f },
    {  99, "099 B54 Auto Release No", "SYNTH FX      ", "ROLAND JUNO",  99, 1,   0,  8,  3000.0f,  2.0f, 100.0f, 1000.0f, 0.70f,  800.0f },
    { 100, "100 B55 Brass Ensemble ", "STRINGS       ", "ROLAND JUNO", 100, 1,   0, 10,  6000.0f,  0.9f, 180.0f,  800.0f, 0.85f,  600.0f },
    { 101, "101 B56 Ethereal       ", "PAD           ", "ROLAND JUNO", 101, 3,   0, 10,  2500.0f,  0.8f, 350.0f,  800.0f, 0.90f, 1000.0f },
    { 102, "102 B57 Chorus Bell 2  ", "PERC/BELL     ", "ROLAND JUNO", 102, 1,   0, 10,  9000.0f,  1.2f,   1.0f, 1000.0f, 0.10f,  400.0f },
    { 103, "103 B58 Blizzard       ", "SYNTH         ", "ROLAND JUNO", 103, 1,   0, 10,  3500.0f,  1.5f,  10.0f,  400.0f, 0.60f,  250.0f },
    { 104, "104 B61 E. Piano with T", "KEYS          ", "ROLAND JUNO", 104, 1,   0, 10,  8000.0f,  1.0f,   2.0f, 1200.0f, 0.30f,  250.0f },
    { 105, "105 B62 Clarinet       ", "WIND          ", "ROLAND JUNO", 105, 1,   0, 10,  4500.0f,  0.9f,  40.0f,  500.0f, 0.85f,  200.0f },
    { 106, "106 B63 Thunder        ", "DRUMS/PERC    ", "ROLAND JUNO", 106, 7,   0,  4,  5000.0f,  1.0f,   1.0f,  350.0f, 0.00f,  150.0f },
    { 107, "107 B64 Reedy Organ    ", "ORGAN         ", "ROLAND JUNO", 107, 1,   0, 10,  7000.0f,  1.0f,   5.0f,  200.0f, 1.00f,  100.0f },
    { 108, "108 B65 Flute / Horn   ", "BRASS         ", "ROLAND JUNO", 108, 1,   0, 10,  4000.0f,  1.0f,  25.0f,  400.0f, 0.80f,  250.0f },
    { 109, "109 B66 Toy Rhodes     ", "KEYS          ", "ROLAND JUNO", 109, 1,   0, 10,  8000.0f,  1.0f,   2.0f, 1200.0f, 0.30f,  250.0f },
    { 110, "110 B67 Surf's Up      ", "SYNTH FX      ", "ROLAND JUNO", 110, 1,   0,  8,  3000.0f,  2.0f, 100.0f, 1000.0f, 0.70f,  800.0f },
    { 111, "111 B68 OW Bass        ", "BASS          ", "ROLAND JUNO", 111, 4, -12,  4,  1200.0f,  2.5f,   5.0f,  250.0f, 0.20f,  100.0f },
    { 112, "112 B71 Piccolo        ", "WIND          ", "ROLAND JUNO", 112, 1,   0, 10,  4500.0f,  0.9f,  40.0f,  500.0f, 0.85f,  200.0f },
    { 113, "113 B72 Melodic Taps   ", "PERC/BELL     ", "ROLAND JUNO", 113, 1,   0, 10,  9000.0f,  1.2f,   1.0f, 1000.0f, 0.10f,  400.0f },
    { 114, "114 B73 Meow Brass     ", "BRASS         ", "ROLAND JUNO", 114, 1,   0, 10,  4000.0f,  1.0f,  25.0f,  400.0f, 0.80f,  250.0f },
    { 115, "115 B74 Violin (high)  ", "STRINGS       ", "ROLAND JUNO", 115, 1,   0, 10,  6000.0f,  0.9f, 180.0f,  800.0f, 0.85f,  600.0f },
    { 116, "116 B75 High Bells     ", "PERC/BELL     ", "ROLAND JUNO", 116, 1,   0, 10,  9000.0f,  1.2f,   1.0f, 1000.0f, 0.10f,  400.0f },
    { 117, "117 B76 Rolling Wah    ", "SYNTH FX      ", "ROLAND JUNO", 117, 1,   0,  8,  3000.0f,  2.0f, 100.0f, 1000.0f, 0.70f,  800.0f },
    { 118, "118 B77 Ping Bell      ", "PERC/BELL     ", "ROLAND JUNO", 118, 1,   0, 10,  9000.0f,  1.2f,   1.0f, 1000.0f, 0.10f,  400.0f },
    { 119, "119 B78 Brassy Organ   ", "BRASS         ", "ROLAND JUNO", 119, 1,   0, 10,  4000.0f,  1.0f,  25.0f,  400.0f, 0.80f,  250.0f },
    { 120, "120 B81 Low Dark String", "STRINGS       ", "ROLAND JUNO", 120, 1,   0, 10,  6000.0f,  0.9f, 180.0f,  800.0f, 0.85f,  600.0f },
    { 121, "121 B82 Piccolo Trumpet", "BRASS         ", "ROLAND JUNO", 121, 1,   0, 10,  4000.0f,  1.0f,  25.0f,  400.0f, 0.80f,  250.0f },
    { 122, "122 B83 Cello          ", "STRINGS       ", "ROLAND JUNO", 122, 1,   0, 10,  6000.0f,  0.9f, 180.0f,  800.0f, 0.85f,  600.0f },
    { 123, "123 B84 High Strings   ", "STRINGS       ", "ROLAND JUNO", 123, 1,   0, 10,  6000.0f,  0.9f, 180.0f,  800.0f, 0.85f,  600.0f },
    { 124, "124 B85 Rocket Men     ", "SYNTH FX      ", "ROLAND JUNO", 124, 1,   0,  8,  3000.0f,  2.0f, 100.0f, 1000.0f, 0.70f,  800.0f },
    { 125, "125 B86 Forbidden Plane", "SYNTH FX      ", "ROLAND JUNO", 125, 1,   0,  8,  3000.0f,  2.0f, 100.0f, 1000.0f, 0.70f,  800.0f },
    { 126, "126 B87 Froggy         ", "SYNTH FX      ", "ROLAND JUNO", 126, 1,   0,  8,  3000.0f,  2.0f, 100.0f, 1000.0f, 0.70f,  800.0f },
    { 127, "127 B88 Owgan          ", "ORGAN         ", "ROLAND JUNO", 127, 1,   0, 10,  7000.0f,  1.0f,   5.0f,  200.0f, 1.00f,  100.0f },

    // ═══════════════════════════════════════════════════════════════════════════════════════
    // Yamaha DX7 6-Operator FM Factory Library (IDs 128..255, ROM 1..4)
    // ═══════════════════════════════════════════════════════════════════════════════════════
    { 128, "128 DX7 BRASS 1        ", "FM BRASS      ", "YAMAHA DX7 ", 128, 8,   0,  8, 10000.0f,  1.0f,   5.0f,  400.0f, 0.70f,  250.0f },
    { 129, "129 DX7 BRASS 2        ", "FM BRASS      ", "YAMAHA DX7 ", 129, 8,   0,  8, 10000.0f,  1.0f,   5.0f,  400.0f, 0.70f,  250.0f },
    { 130, "130 DX7 BRASS 3        ", "FM BRASS      ", "YAMAHA DX7 ", 130, 8,   0,  8, 10000.0f,  1.0f,   5.0f,  400.0f, 0.70f,  250.0f },
    { 131, "131 DX7 STRINGS 1      ", "FM STRINGS    ", "YAMAHA DX7 ", 131, 8,   0,  8,  8500.0f,  0.9f, 180.0f,  800.0f, 0.85f,  600.0f },
    { 132, "132 DX7 STRINGS 2      ", "FM STRINGS    ", "YAMAHA DX7 ", 132, 8,   0,  8,  8500.0f,  0.9f, 180.0f,  800.0f, 0.85f,  600.0f },
    { 133, "133 DX7 STRINGS 3      ", "FM STRINGS    ", "YAMAHA DX7 ", 133, 8,   0,  8,  8500.0f,  0.9f, 180.0f,  800.0f, 0.85f,  600.0f },
    { 134, "134 DX7 ORCHESTRA      ", "FM ORCHESTRA  ", "YAMAHA DX7 ", 134, 8,   0,  8, 11000.0f,  1.0f,  50.0f,  600.0f, 0.80f,  500.0f },
    { 135, "135 DX7 PIANO 1        ", "FM PIANO      ", "YAMAHA DX7 ", 135, 8,   0,  8, 14000.0f,  1.0f,   1.0f, 1500.0f, 0.30f,  300.0f },
    { 136, "136 DX7 PIANO 2        ", "FM PIANO      ", "YAMAHA DX7 ", 136, 8,   0,  8, 14000.0f,  1.0f,   1.0f, 1500.0f, 0.30f,  300.0f },
    { 137, "137 DX7 PIANO 3        ", "FM PIANO      ", "YAMAHA DX7 ", 137, 8,   0,  8, 14000.0f,  1.0f,   1.0f, 1500.0f, 0.30f,  300.0f },
    { 138, "138 DX7 E.PIANO 1      ", "FM E.PIANO    ", "YAMAHA DX7 ", 138, 8,   0,  8, 12000.0f,  1.0f,   1.0f, 1200.0f, 0.40f,  350.0f },
    { 139, "139 DX7 GUITAR 1       ", "FM GUITAR     ", "YAMAHA DX7 ", 139, 8,   0,  8,  9000.0f,  1.0f,   1.0f,  800.0f, 0.20f,  200.0f },
    { 140, "140 DX7 GUITAR 2       ", "FM GUITAR     ", "YAMAHA DX7 ", 140, 8,   0,  8,  9000.0f,  1.0f,   1.0f,  800.0f, 0.20f,  200.0f },
    { 141, "141 DX7 SYN-LEAD 1     ", "FM SYNTH      ", "YAMAHA DX7 ", 141, 8,   0,  8, 12000.0f,  1.0f,   5.0f,  300.0f, 0.80f,  200.0f },
    { 142, "142 DX7 BASS 1         ", "FM BASS       ", "YAMAHA DX7 ", 142, 8, -12,  4,  3500.0f,  2.0f,   1.0f,  300.0f, 0.10f,  150.0f },
    { 143, "143 DX7 BASS 2         ", "FM BASS       ", "YAMAHA DX7 ", 143, 8, -12,  4,  3500.0f,  2.0f,   1.0f,  300.0f, 0.10f,  150.0f },
    { 144, "144 DX7 E.ORGAN 1      ", "FM ORGAN      ", "YAMAHA DX7 ", 144, 8,   0,  8, 10000.0f,  1.0f,   1.0f,  200.0f, 1.00f,  100.0f },
    { 145, "145 DX7 PIPES 1        ", "FM ORGAN      ", "YAMAHA DX7 ", 145, 8,   0,  8, 10000.0f,  1.0f,   1.0f,  200.0f, 1.00f,  100.0f },
    { 146, "146 DX7 HARPSICH 1     ", "FM KEYS       ", "YAMAHA DX7 ", 146, 8,   0,  8,  9000.0f,  1.5f,   1.0f,  250.0f, 0.00f,  120.0f },
    { 147, "147 DX7 CLAV 1         ", "FM KEYS       ", "YAMAHA DX7 ", 147, 8,   0,  8,  9000.0f,  1.5f,   1.0f,  250.0f, 0.00f,  120.0f },
    { 148, "148 DX7 VIBE 1         ", "FM PERC       ", "YAMAHA DX7 ", 148, 8,   0,  8, 10000.0f,  1.0f,   1.0f,  450.0f, 0.00f,  200.0f },
    { 149, "149 DX7 MARIMBA        ", "FM PERC       ", "YAMAHA DX7 ", 149, 8,   0,  8, 10000.0f,  1.0f,   1.0f,  450.0f, 0.00f,  200.0f },
    { 150, "150 DX7 KOTO           ", "FM ETHNIC     ", "YAMAHA DX7 ", 150, 8,   0,  8, 10000.0f,  1.2f,   1.0f,  400.0f, 0.00f,  200.0f },
    { 151, "151 DX7 FLUTE 1        ", "FM WIND       ", "YAMAHA DX7 ", 151, 8,   0,  8,  7000.0f,  1.0f,  20.0f,  400.0f, 0.90f,  200.0f },
    { 152, "152 DX7 ORCH-CHIME     ", "FM ORCHESTRA  ", "YAMAHA DX7 ", 152, 8,   0,  8, 11000.0f,  1.0f,  50.0f,  600.0f, 0.80f,  500.0f },
    { 153, "153 DX7 TUB BELLS      ", "FM BELL       ", "YAMAHA DX7 ", 153, 8,   0,  8, 14000.0f,  1.0f,   1.0f, 2000.0f, 0.00f,  800.0f },
    { 154, "154 DX7 STEEL DRUM     ", "FM PERC       ", "YAMAHA DX7 ", 154, 8,   0,  8, 10000.0f,  1.0f,   1.0f,  450.0f, 0.00f,  200.0f },
    { 155, "155 DX7 TIMPANI        ", "FM PERC       ", "YAMAHA DX7 ", 155, 8,   0,  8, 10000.0f,  1.0f,   1.0f,  450.0f, 0.00f,  200.0f },
    { 156, "156 DX7 REFS WHISL     ", "FM FX         ", "YAMAHA DX7 ", 156, 8,   0,  8, 10000.0f,  1.2f,  50.0f,  800.0f, 0.50f,  400.0f },
    { 157, "157 DX7 VOICE 1        ", "FM VOICE      ", "YAMAHA DX7 ", 157, 8,   0,  8,  7000.0f,  1.0f,  80.0f,  800.0f, 0.90f,  400.0f },
    { 158, "158 DX7 TRAIN          ", "FM FX         ", "YAMAHA DX7 ", 158, 8,   0,  8, 10000.0f,  1.2f,  50.0f,  800.0f, 0.50f,  400.0f },
    { 159, "159 DX7 TAKE OFF       ", "FM FX         ", "YAMAHA DX7 ", 159, 8,   0,  8, 10000.0f,  1.2f,  50.0f,  800.0f, 0.50f,  400.0f },
    { 160, "160 DX7 PIANO 4        ", "FM PIANO      ", "YAMAHA DX7 ", 160, 8,   0,  8, 14000.0f,  1.0f,   1.0f, 1500.0f, 0.30f,  300.0f },
    { 161, "161 DX7 PIANO 5        ", "FM PIANO      ", "YAMAHA DX7 ", 161, 8,   0,  8, 14000.0f,  1.0f,   1.0f, 1500.0f, 0.30f,  300.0f },
    { 162, "162 DX7 E.PIANO 2      ", "FM E.PIANO    ", "YAMAHA DX7 ", 162, 8,   0,  8, 12000.0f,  1.0f,   1.0f, 1200.0f, 0.40f,  350.0f },
    { 163, "163 DX7 E.PIANO 3      ", "FM E.PIANO    ", "YAMAHA DX7 ", 163, 8,   0,  8, 12000.0f,  1.0f,   1.0f, 1200.0f, 0.40f,  350.0f },
    { 164, "164 DX7 E.PIANO 4      ", "FM E.PIANO    ", "YAMAHA DX7 ", 164, 8,   0,  8, 12000.0f,  1.0f,   1.0f, 1200.0f, 0.40f,  350.0f },
    { 165, "165 DX7 PIANO 5THS     ", "FM PIANO      ", "YAMAHA DX7 ", 165, 8,   0,  8, 14000.0f,  1.0f,   1.0f, 1500.0f, 0.30f,  300.0f },
    { 166, "166 DX7 CELESTE        ", "FM BELL       ", "YAMAHA DX7 ", 166, 8,   0,  8, 14000.0f,  1.0f,   1.0f, 2000.0f, 0.00f,  800.0f },
    { 167, "167 DX7 TOY PIANO      ", "FM PIANO      ", "YAMAHA DX7 ", 167, 8,   0,  8, 14000.0f,  1.0f,   1.0f, 1500.0f, 0.30f,  300.0f },
    { 168, "168 DX7 HARPSICH 2     ", "FM KEYS       ", "YAMAHA DX7 ", 168, 8,   0,  8,  9000.0f,  1.5f,   1.0f,  250.0f, 0.00f,  120.0f },
    { 169, "169 DX7 HARPSICH 3     ", "FM KEYS       ", "YAMAHA DX7 ", 169, 8,   0,  8,  9000.0f,  1.5f,   1.0f,  250.0f, 0.00f,  120.0f },
    { 170, "170 DX7 CLAV 2         ", "FM KEYS       ", "YAMAHA DX7 ", 170, 8,   0,  8,  9000.0f,  1.5f,   1.0f,  250.0f, 0.00f,  120.0f },
    { 171, "171 DX7 CLAV 3         ", "FM KEYS       ", "YAMAHA DX7 ", 171, 8,   0,  8,  9000.0f,  1.5f,   1.0f,  250.0f, 0.00f,  120.0f },
    { 172, "172 DX7 E.ORGAN 2      ", "FM ORGAN      ", "YAMAHA DX7 ", 172, 8,   0,  8, 10000.0f,  1.0f,   1.0f,  200.0f, 1.00f,  100.0f },
    { 173, "173 DX7 E.ORGAN 3      ", "FM ORGAN      ", "YAMAHA DX7 ", 173, 8,   0,  8, 10000.0f,  1.0f,   1.0f,  200.0f, 1.00f,  100.0f },
    { 174, "174 DX7 E.ORGAN 4      ", "FM ORGAN      ", "YAMAHA DX7 ", 174, 8,   0,  8, 10000.0f,  1.0f,   1.0f,  200.0f, 1.00f,  100.0f },
    { 175, "175 DX7 E.ORGAN 5      ", "FM ORGAN      ", "YAMAHA DX7 ", 175, 8,   0,  8, 10000.0f,  1.0f,   1.0f,  200.0f, 1.00f,  100.0f },
    { 176, "176 DX7 PIPES 2        ", "FM ORGAN      ", "YAMAHA DX7 ", 176, 8,   0,  8, 10000.0f,  1.0f,   1.0f,  200.0f, 1.00f,  100.0f },
    { 177, "177 DX7 PIPES 3        ", "FM ORGAN      ", "YAMAHA DX7 ", 177, 8,   0,  8, 10000.0f,  1.0f,   1.0f,  200.0f, 1.00f,  100.0f },
    { 178, "178 DX7 PIPES 4        ", "FM ORGAN      ", "YAMAHA DX7 ", 178, 8,   0,  8, 10000.0f,  1.0f,   1.0f,  200.0f, 1.00f,  100.0f },
    { 179, "179 DX7 CALIOPE        ", "FM FX         ", "YAMAHA DX7 ", 179, 8,   0,  8, 10000.0f,  1.2f,  50.0f,  800.0f, 0.50f,  400.0f },
    { 180, "180 DX7 ACCORDION      ", "FM KEYS       ", "YAMAHA DX7 ", 180, 8,   0,  8,  9000.0f,  1.5f,   1.0f,  250.0f, 0.00f,  120.0f },
    { 181, "181 DX7 SITAR          ", "FM ETHNIC     ", "YAMAHA DX7 ", 181, 8,   0,  8, 10000.0f,  1.2f,   1.0f,  400.0f, 0.00f,  200.0f },
    { 182, "182 DX7 GUITAR 3       ", "FM GUITAR     ", "YAMAHA DX7 ", 182, 8,   0,  8,  9000.0f,  1.0f,   1.0f,  800.0f, 0.20f,  200.0f },
    { 183, "183 DX7 GUITAR 4       ", "FM GUITAR     ", "YAMAHA DX7 ", 183, 8,   0,  8,  9000.0f,  1.0f,   1.0f,  800.0f, 0.20f,  200.0f },
    { 184, "184 DX7 GUITAR 5       ", "FM GUITAR     ", "YAMAHA DX7 ", 184, 8,   0,  8,  9000.0f,  1.0f,   1.0f,  800.0f, 0.20f,  200.0f },
    { 185, "185 DX7 GUITAR 6       ", "FM GUITAR     ", "YAMAHA DX7 ", 185, 8,   0,  8,  9000.0f,  1.0f,   1.0f,  800.0f, 0.20f,  200.0f },
    { 186, "186 DX7 LUTE           ", "FM ETHNIC     ", "YAMAHA DX7 ", 186, 8,   0,  8, 10000.0f,  1.2f,   1.0f,  400.0f, 0.00f,  200.0f },
    { 187, "187 DX7 BANJO          ", "FM ETHNIC     ", "YAMAHA DX7 ", 187, 8,   0,  8, 10000.0f,  1.2f,   1.0f,  400.0f, 0.00f,  200.0f },
    { 188, "188 DX7 HARP 1         ", "FM ETHNIC     ", "YAMAHA DX7 ", 188, 8,   0,  8, 10000.0f,  1.2f,   1.0f,  400.0f, 0.00f,  200.0f },
    { 189, "189 DX7 HARP 2         ", "FM ETHNIC     ", "YAMAHA DX7 ", 189, 8,   0,  8, 10000.0f,  1.2f,   1.0f,  400.0f, 0.00f,  200.0f },
    { 190, "190 DX7 BASS 3         ", "FM BASS       ", "YAMAHA DX7 ", 190, 8, -12,  4,  3500.0f,  2.0f,   1.0f,  300.0f, 0.10f,  150.0f },
    { 191, "191 DX7 BASS 4         ", "FM BASS       ", "YAMAHA DX7 ", 191, 8, -12,  4,  3500.0f,  2.0f,   1.0f,  300.0f, 0.10f,  150.0f },
    { 192, "192 DX7 PICCOLO        ", "FM WIND       ", "YAMAHA DX7 ", 192, 8,   0,  8,  7000.0f,  1.0f,  20.0f,  400.0f, 0.90f,  200.0f },
    { 193, "193 DX7 FLUTE 2        ", "FM WIND       ", "YAMAHA DX7 ", 193, 8,   0,  8,  7000.0f,  1.0f,  20.0f,  400.0f, 0.90f,  200.0f },
    { 194, "194 DX7 OBOE           ", "FM WIND       ", "YAMAHA DX7 ", 194, 8,   0,  8,  7000.0f,  1.0f,  20.0f,  400.0f, 0.90f,  200.0f },
    { 195, "195 DX7 CLARINET       ", "FM WIND       ", "YAMAHA DX7 ", 195, 8,   0,  8,  7000.0f,  1.0f,  20.0f,  400.0f, 0.90f,  200.0f },
    { 196, "196 DX7 SAX BC         ", "FM WIND       ", "YAMAHA DX7 ", 196, 8,   0,  8,  7000.0f,  1.0f,  20.0f,  400.0f, 0.90f,  200.0f },
    { 197, "197 DX7 BASSOON        ", "FM BASS       ", "YAMAHA DX7 ", 197, 8, -12,  4,  3500.0f,  2.0f,   1.0f,  300.0f, 0.10f,  150.0f },
    { 198, "198 DX7 STRINGS 4      ", "FM STRINGS    ", "YAMAHA DX7 ", 198, 8,   0,  8,  8500.0f,  0.9f, 180.0f,  800.0f, 0.85f,  600.0f },
    { 199, "199 DX7 STRINGS 5      ", "FM STRINGS    ", "YAMAHA DX7 ", 199, 8,   0,  8,  8500.0f,  0.9f, 180.0f,  800.0f, 0.85f,  600.0f },
    { 200, "200 DX7 STRINGS 6      ", "FM STRINGS    ", "YAMAHA DX7 ", 200, 8,   0,  8,  8500.0f,  0.9f, 180.0f,  800.0f, 0.85f,  600.0f },
    { 201, "201 DX7 STRINGS 7      ", "FM STRINGS    ", "YAMAHA DX7 ", 201, 8,   0,  8,  8500.0f,  0.9f, 180.0f,  800.0f, 0.85f,  600.0f },
    { 202, "202 DX7 STRINGS 8      ", "FM STRINGS    ", "YAMAHA DX7 ", 202, 8,   0,  8,  8500.0f,  0.9f, 180.0f,  800.0f, 0.85f,  600.0f },
    { 203, "203 DX7 BRASS 4        ", "FM BRASS      ", "YAMAHA DX7 ", 203, 8,   0,  8, 10000.0f,  1.0f,   5.0f,  400.0f, 0.70f,  250.0f },
    { 204, "204 DX7 BRASS 5        ", "FM BRASS      ", "YAMAHA DX7 ", 204, 8,   0,  8, 10000.0f,  1.0f,   5.0f,  400.0f, 0.70f,  250.0f },
    { 205, "205 DX7 BRASS 6 BC     ", "FM BRASS      ", "YAMAHA DX7 ", 205, 8,   0,  8, 10000.0f,  1.0f,   5.0f,  400.0f, 0.70f,  250.0f },
    { 206, "206 DX7 BRASS 7        ", "FM BRASS      ", "YAMAHA DX7 ", 206, 8,   0,  8, 10000.0f,  1.0f,   5.0f,  400.0f, 0.70f,  250.0f },
    { 207, "207 DX7 BRASS 8        ", "FM BRASS      ", "YAMAHA DX7 ", 207, 8,   0,  8, 10000.0f,  1.0f,   5.0f,  400.0f, 0.70f,  250.0f },
    { 208, "208 DX7 RECORDER       ", "FM WIND       ", "YAMAHA DX7 ", 208, 8,   0,  8,  7000.0f,  1.0f,  20.0f,  400.0f, 0.90f,  200.0f },
    { 209, "209 DX7 HARMONICA1     ", "FM WIND       ", "YAMAHA DX7 ", 209, 8,   0,  8,  7000.0f,  1.0f,  20.0f,  400.0f, 0.90f,  200.0f },
    { 210, "210 DX7 HRMNCA2 BC     ", "FM WIND       ", "YAMAHA DX7 ", 210, 8,   0,  8,  7000.0f,  1.0f,  20.0f,  400.0f, 0.90f,  200.0f },
    { 211, "211 DX7 VOICE 2        ", "FM VOICE      ", "YAMAHA DX7 ", 211, 8,   0,  8,  7000.0f,  1.0f,  80.0f,  800.0f, 0.90f,  400.0f },
    { 212, "212 DX7 VOICE 3        ", "FM VOICE      ", "YAMAHA DX7 ", 212, 8,   0,  8,  7000.0f,  1.0f,  80.0f,  800.0f, 0.90f,  400.0f },
    { 213, "213 DX7 GLOKENSPL      ", "FM BELL       ", "YAMAHA DX7 ", 213, 8,   0,  8, 14000.0f,  1.0f,   1.0f, 2000.0f, 0.00f,  800.0f },
    { 214, "214 DX7 VIBE 2         ", "FM PERC       ", "YAMAHA DX7 ", 214, 8,   0,  8, 10000.0f,  1.0f,   1.0f,  450.0f, 0.00f,  200.0f },
    { 215, "215 DX7 XYLOPHONE      ", "FM PERC       ", "YAMAHA DX7 ", 215, 8,   0,  8, 10000.0f,  1.0f,   1.0f,  450.0f, 0.00f,  200.0f },
    { 216, "216 DX7 CHIMES         ", "FM BELL       ", "YAMAHA DX7 ", 216, 8,   0,  8, 14000.0f,  1.0f,   1.0f, 2000.0f, 0.00f,  800.0f },
    { 217, "217 DX7 GONG 1         ", "FM FX         ", "YAMAHA DX7 ", 217, 8,   0,  8, 10000.0f,  1.2f,  50.0f,  800.0f, 0.50f,  400.0f },
    { 218, "218 DX7 GONG 2         ", "FM FX         ", "YAMAHA DX7 ", 218, 8,   0,  8, 10000.0f,  1.2f,  50.0f,  800.0f, 0.50f,  400.0f },
    { 219, "219 DX7 BELLS          ", "FM BELL       ", "YAMAHA DX7 ", 219, 8,   0,  8, 14000.0f,  1.0f,   1.0f, 2000.0f, 0.00f,  800.0f },
    { 220, "220 DX7 COW BELL       ", "FM BELL       ", "YAMAHA DX7 ", 220, 8,   0,  8, 14000.0f,  1.0f,   1.0f, 2000.0f, 0.00f,  800.0f },
    { 221, "221 DX7 BLOCK          ", "FM PERC       ", "YAMAHA DX7 ", 221, 8,   0,  8, 10000.0f,  1.0f,   1.0f,  450.0f, 0.00f,  200.0f },
    { 222, "222 DX7 FLEXATONE      ", "FM FX         ", "YAMAHA DX7 ", 222, 8,   0,  8, 10000.0f,  1.2f,  50.0f,  800.0f, 0.50f,  400.0f },
    { 223, "223 DX7 LOG DRUM       ", "FM PERC       ", "YAMAHA DX7 ", 223, 8,   0,  8, 10000.0f,  1.0f,   1.0f,  450.0f, 0.00f,  200.0f },
    { 224, "224 DX7 SYN-LEAD 2     ", "FM SYNTH      ", "YAMAHA DX7 ", 224, 8,   0,  8, 12000.0f,  1.0f,   5.0f,  300.0f, 0.80f,  200.0f },
    { 225, "225 DX7 SYN-LEAD 3     ", "FM SYNTH      ", "YAMAHA DX7 ", 225, 8,   0,  8, 12000.0f,  1.0f,   5.0f,  300.0f, 0.80f,  200.0f },
    { 226, "226 DX7 SYN-LEAD 4     ", "FM SYNTH      ", "YAMAHA DX7 ", 226, 8,   0,  8, 12000.0f,  1.0f,   5.0f,  300.0f, 0.80f,  200.0f },
    { 227, "227 DX7 SYN-LEAD 5     ", "FM SYNTH      ", "YAMAHA DX7 ", 227, 8,   0,  8, 12000.0f,  1.0f,   5.0f,  300.0f, 0.80f,  200.0f },
    { 228, "228 DX7 SYN-CLAV 1     ", "FM KEYS       ", "YAMAHA DX7 ", 228, 8,   0,  8,  9000.0f,  1.5f,   1.0f,  250.0f, 0.00f,  120.0f },
    { 229, "229 DX7 SYN-CLAV 2     ", "FM KEYS       ", "YAMAHA DX7 ", 229, 8,   0,  8,  9000.0f,  1.5f,   1.0f,  250.0f, 0.00f,  120.0f },
    { 230, "230 DX7 SYN-CLAV 3     ", "FM KEYS       ", "YAMAHA DX7 ", 230, 8,   0,  8,  9000.0f,  1.5f,   1.0f,  250.0f, 0.00f,  120.0f },
    { 231, "231 DX7 SYN-PIANO      ", "FM PIANO      ", "YAMAHA DX7 ", 231, 8,   0,  8, 14000.0f,  1.0f,   1.0f, 1500.0f, 0.30f,  300.0f },
    { 232, "232 DX7 SYNBRASS 1     ", "FM BRASS      ", "YAMAHA DX7 ", 232, 8,   0,  8, 10000.0f,  1.0f,   5.0f,  400.0f, 0.70f,  250.0f },
    { 233, "233 DX7 SYNBRASS 2     ", "FM BRASS      ", "YAMAHA DX7 ", 233, 8,   0,  8, 10000.0f,  1.0f,   5.0f,  400.0f, 0.70f,  250.0f },
    { 234, "234 DX7 SYNORGAN 1     ", "FM ORGAN      ", "YAMAHA DX7 ", 234, 8,   0,  8, 10000.0f,  1.0f,   1.0f,  200.0f, 1.00f,  100.0f },
    { 235, "235 DX7 SYNORGAN 2     ", "FM ORGAN      ", "YAMAHA DX7 ", 235, 8,   0,  8, 10000.0f,  1.0f,   1.0f,  200.0f, 1.00f,  100.0f },
    { 236, "236 DX7 SYN-VOX        ", "FM VOICE      ", "YAMAHA DX7 ", 236, 8,   0,  8,  7000.0f,  1.0f,  80.0f,  800.0f, 0.90f,  400.0f },
    { 237, "237 DX7 SYN-ORCH       ", "FM ORCHESTRA  ", "YAMAHA DX7 ", 237, 8,   0,  8, 11000.0f,  1.0f,  50.0f,  600.0f, 0.80f,  500.0f },
    { 238, "238 DX7 SYN-BASS 1     ", "FM BASS       ", "YAMAHA DX7 ", 238, 8, -12,  4,  3500.0f,  2.0f,   1.0f,  300.0f, 0.10f,  150.0f },
    { 239, "239 DX7 SYN-BASS 2     ", "FM BASS       ", "YAMAHA DX7 ", 239, 8, -12,  4,  3500.0f,  2.0f,   1.0f,  300.0f, 0.10f,  150.0f },
    { 240, "240 DX7 HARP-FLUTE     ", "FM WIND       ", "YAMAHA DX7 ", 240, 8,   0,  8,  7000.0f,  1.0f,  20.0f,  400.0f, 0.90f,  200.0f },
    { 241, "241 DX7 BELL-FLUTE     ", "FM BELL       ", "YAMAHA DX7 ", 241, 8,   0,  8, 14000.0f,  1.0f,   1.0f, 2000.0f, 0.00f,  800.0f },
    { 242, "242 DX7 E.P-BRS BC     ", "FM FX         ", "YAMAHA DX7 ", 242, 8,   0,  8, 10000.0f,  1.2f,  50.0f,  800.0f, 0.50f,  400.0f },
    { 243, "243 DX7 T.BL-EXPA      ", "FM FX         ", "YAMAHA DX7 ", 243, 8,   0,  8, 10000.0f,  1.2f,  50.0f,  800.0f, 0.50f,  400.0f },
    { 244, "244 DX7 CHIME-STRG     ", "FM BELL       ", "YAMAHA DX7 ", 244, 8,   0,  8, 14000.0f,  1.0f,   1.0f, 2000.0f, 0.00f,  800.0f },
    { 245, "245 DX7 B.DRM-SNAR     ", "FM DRUMS      ", "YAMAHA DX7 ", 245, 8,   0,  8,  8000.0f,  1.0f,   1.0f,  300.0f, 0.00f,  100.0f },
    { 246, "246 DX7 SHIMMER        ", "FM PAD        ", "YAMAHA DX7 ", 246, 8,   0,  8, 10000.0f,  1.0f, 150.0f,  800.0f, 0.90f,  800.0f },
    { 247, "247 DX7 EVOLUTION      ", "FM PAD        ", "YAMAHA DX7 ", 247, 8,   0,  8, 10000.0f,  1.0f, 150.0f,  800.0f, 0.90f,  800.0f },
    { 248, "248 DX7 WATER GDN      ", "FM FX         ", "YAMAHA DX7 ", 248, 8,   0,  8, 10000.0f,  1.2f,  50.0f,  800.0f, 0.50f,  400.0f },
    { 249, "249 DX7 WASP STING     ", "FM FX         ", "YAMAHA DX7 ", 249, 8,   0,  8, 10000.0f,  1.2f,  50.0f,  800.0f, 0.50f,  400.0f },
    { 250, "250 DX7 LASER GUN      ", "FM FX         ", "YAMAHA DX7 ", 250, 8,   0,  8, 10000.0f,  1.2f,  50.0f,  800.0f, 0.50f,  400.0f },
    { 251, "251 DX7 DESCENT        ", "FM FX         ", "YAMAHA DX7 ", 251, 8,   0,  8, 10000.0f,  1.2f,  50.0f,  800.0f, 0.50f,  400.0f },
    { 252, "252 DX7 OCTAVE WAR     ", "FM FX         ", "YAMAHA DX7 ", 252, 8,   0,  8, 10000.0f,  1.2f,  50.0f,  800.0f, 0.50f,  400.0f },
    { 253, "253 DX7 GRAND PRIX     ", "FM FX         ", "YAMAHA DX7 ", 253, 8,   0,  8, 10000.0f,  1.2f,  50.0f,  800.0f, 0.50f,  400.0f },
    { 254, "254 DX7 ST.HELENS      ", "FM FX         ", "YAMAHA DX7 ", 254, 8,   0,  8, 10000.0f,  1.2f,  50.0f,  800.0f, 0.50f,  400.0f },
    { 255, "255 DX7 EXPLOSION      ", "FM FX         ", "YAMAHA DX7 ", 255, 8,   0,  8, 10000.0f,  1.2f,  50.0f,  800.0f, 0.50f,  400.0f },
};

static void buildPatchFromDescriptor(const PatchDescriptor& desc, SynthPatch& out) {
    out = {};
    out.id = desc.id;
    strncpy(out.name, desc.name, sizeof(out.name) - 1);
    strncpy(out.category, desc.category, sizeof(out.category) - 1);
    strncpy(out.author, desc.author, sizeof(out.author) - 1);
    out.engine_patch = desc.engine_patch;
    out.wave_type = desc.wave_type;
    out.transpose = desc.transpose;
    out.voice_count = desc.voice_count;
    out.base_freq = 440.0f;
    out.filter_cutoff = desc.filter_cutoff;
    out.filter_res = desc.filter_res;
    out.amp_attack = desc.amp_attack;
    out.amp_decay = desc.amp_decay;
    out.amp_sustain = desc.amp_sustain;
    out.amp_release = desc.amp_release;

    // Configure standard clean macros
    out.macros[0] = makeMacro("CHAR", 50.0f, 0, 500.0f, 12000.0f);
    out.macros[1] = makeMacro("BRTE", 50.0f, 2, 200.0f, 14000.0f);
    out.macros[2] = makeMacro("MOTN",  0.0f, 5,   0.0f,     1.0f);
    out.macros[3] = makeMacro("SHAP", 40.0f, 3,   5.0f,   500.0f);
    out.macros[4] = makeMacro("ATK",  10.0f, 3,   1.0f,   200.0f);
    out.macros[5] = makeMacro("REL",  30.0f, 4,  10.0f,  1000.0f);
    out.macros[6] = makeMacro("SPCE",  0.0f, 6,   0.0f,     0.5f); // Clean default reverb (no noise leak)
    out.macros[7] = makeMacro("DRV",   0.0f, 7,   0.0f,    0.16f); // Safe FM feedback / drive

    out.crc32 = calculatePatchCrc32(out);
}

static SynthPatch s_patch_cache;

const SynthPatch* FactoryPatches::getPatchById(uint8_t patch_id) {
    for (size_t i = 0; i < kCount; ++i) {
        if (s_patch_table[i].id == patch_id) {
            buildPatchFromDescriptor(s_patch_table[i], s_patch_cache);
            return &s_patch_cache;
        }
    }
    buildPatchFromDescriptor(s_patch_table[0], s_patch_cache);
    return &s_patch_cache;
}

const SynthPatch* FactoryPatches::getPatchByIndex(size_t index) {
    if (index >= kCount) {
        buildPatchFromDescriptor(s_patch_table[0], s_patch_cache);
    } else {
        buildPatchFromDescriptor(s_patch_table[index], s_patch_cache);
    }
    return &s_patch_cache;
}

} // namespace smk
