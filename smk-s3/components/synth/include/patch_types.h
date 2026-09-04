#pragma once
#include <cstdint>

namespace smk {

enum class KnobBank : uint8_t {
    BankA_Macros     = 0, // 8 Macros
    BankB_Oscillator = 1, // Osc mix, wave, detune, octave, sub, noise, FM, mod
    BankC_FilterEnv  = 2, // Cutoff, res, env amt, attack, decay, sustain, release, keytrack
    BankD_Effects    = 3, // Chorus, delay time, delay fb, delay mix, reverb size, reverb mix, drive, master tone
    BankE_Sequencer  = 4  // BPM, swing, gate, prob, ratchet, length, transpose, pattern
};

enum class MacroId : uint8_t {
    Character  = 0, // CHAR: Cutoff / Resonance / Filter Mode
    Brightness = 1, // BRTE: Osc Waveform / Harmonics / Pulse Width
    Motion     = 2, // MOTN: LFO Rate / Modulation Depth
    Shape      = 3, // SHAP: Envelope Attack/Decay Contour
    Attack     = 4, // ATK : Amp Envelope Attack Time
    Release    = 5, // REL : Amp Envelope Release Time
    Space      = 6, // SPCE: Reverb / Delay Send Level
    Drive      = 7  // DRV : Overdrive / Distortion / Gain
};

struct MacroMapping {
    uint16_t osc_target;  // AMY Osc target (0xFFFF = all active oscs)
    uint8_t  param_type;  // AMY parameter type (0=Cutoff, 1=Res, 2=Wave, 3=AmpAttack, 4=AmpRelease, 5=LfoRate, 6=LfoDepth, 7=Feedback)
    float    min_val;     // Minimum value at Macro = 0.0
    float    max_val;     // Maximum value at Macro = 100.0
    uint8_t  curve_type;  // 0 = Linear, 1 = Exponential, 2 = Logarithmic
};

struct MacroConfig {
    char         name[8];
    float        default_val;
    float        current_val;
    MacroMapping mappings[4];
    uint8_t      mapping_count;
};

constexpr uint32_t kPatchMagic = 0x534D4B31; // "SMK1"
constexpr uint16_t kPatchFormatVersion = 3;

struct PatchHeader {
    uint32_t magic;          // 0x534D4B31
    uint16_t format_version; // Version 3
    uint16_t data_size;      // Payload data size
    uint32_t crc32;          // Checksum of patch data
};

struct SynthPatch {
    uint8_t     id;
    char        name[24];
    char        category[16];
    char        author[16];
    uint16_t    engine_patch; // AMY preset or patch ID (0..127 Juno, 128..255 DX7, 256+ PCM)
    int8_t      transpose;    // Transpose in semitones (-24..+24)
    uint8_t     voice_count;  // Max polyphony voices (e.g. 8)
    uint8_t     wave_type;    // 0=SINE, 1=SAW_DOWN, 2=SAW_UP, 3=TRIANGLE, 4=SQUARE, 5=NOISE, 6=KS, 7=PCM, 8=ALGO
    uint8_t     mono_mode;    // 0=Polyphonic, 1=Monophonic Legato
    uint16_t    portamento_ms;// Portamento glide time in milliseconds
    float       base_freq;
    float       filter_cutoff;
    float       filter_res;
    float       amp_attack;
    float       amp_decay;
    float       amp_sustain;
    float       amp_release;
    MacroConfig macros[8];
    uint32_t    crc32;
};

// Helper function to calculate CRC32 checksum for a SynthPatch structure
uint32_t calculatePatchCrc32(const SynthPatch& patch);

} // namespace smk
