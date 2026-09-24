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
constexpr uint16_t kPatchFormatVersion = 5;

struct PatchHeader {
    uint32_t magic;          // 0x534D4B31
    uint16_t format_version; // Version 5
    uint16_t data_size;      // Payload data size
    uint32_t crc32;          // Checksum of patch data
};

struct FxControlState {
    float   chorus_depth   = 0.0f;   // 0.0 .. 1.0
    uint8_t chorus_mode    = 0;      // 0=Off, 1=Classic, 2=Juno, 3=Ensemble, 4=Wide, 5=Vibrato

    float   delay_time_ms  = 350.0f; // 10.0 .. 1000.0 ms (or synced)
    float   delay_feedback = 0.4f;   // 0.0 .. 0.95
    float   delay_mix      = 0.0f;   // 0.0 .. 1.0

    float   reverb_size    = 0.7f;   // 0.0 .. 1.0
    float   reverb_mix     = 0.0f;   // 0.0 .. 1.0

    float   drive          = 0.0f;   // 0.0 .. 1.0 (post-FX synth bus saturation)
    float   master_tone    = 0.0f;   // -1.0 .. +1.0
};

enum class SmkFilterType : uint8_t {
    Inherit = 0xFF, // Preserves AMY preset filter type
    None    = 0,    // FILTER_NONE
    LPF     = 1,    // FILTER_LPF (12dB biquad)
    BPF     = 2,    // FILTER_BPF
    HPF     = 3,    // FILTER_HPF
    LPF24   = 4,    // FILTER_LPF24 (24dB 4-pole)
    Notch   = 5,    // FILTER_NOTCH
    Phaser  = 6,    // FILTER_PHASER
    Moog24  = 7,    // FILTER_MOOG24
    TptSvf  = 8     // FILTER_TPT_SVF
};

inline uint8_t toAmyFilterType(SmkFilterType type) {
    return static_cast<uint8_t>(type);
}

inline SmkFilterType fromAmyFilterType(uint8_t amy_type) {
    if (amy_type == 0xFF) return SmkFilterType::Inherit;
    if (amy_type > 8) return SmkFilterType::None;
    return static_cast<SmkFilterType>(amy_type);
}

inline const char* filterTypeName(SmkFilterType type) {
    switch (type) {
        case SmkFilterType::Inherit: return "PRESET";
        case SmkFilterType::None:   return "NONE";
        case SmkFilterType::LPF:    return "LPF 12dB";
        case SmkFilterType::BPF:    return "BPF";
        case SmkFilterType::HPF:    return "HPF";
        case SmkFilterType::LPF24:  return "LPF 24dB";
        case SmkFilterType::Notch:  return "NOTCH";
        case SmkFilterType::Phaser: return "PHASER";
        case SmkFilterType::Moog24: return "MOOG 24";
        case SmkFilterType::TptSvf: return "TPT SVF";
        default:                    return "UNKNOWN";
    }
}

enum class SmkWaveType : uint8_t {
    Sine          = 0, // SINE
    Pulse         = 1, // PULSE / SQUARE
    SawDown       = 2, // SAW_DOWN
    SawUp         = 3, // SAW_UP
    Triangle      = 4, // TRIANGLE
    Noise         = 5, // NOISE
    KarplusStrong = 6, // KS
    Pcm           = 7, // PCM
    Algo          = 8  // ALGO (FM 6-op)
};

inline uint8_t toAmyWaveType(SmkWaveType type) {
    return static_cast<uint8_t>(type);
}

inline SmkWaveType fromAmyWaveType(uint8_t amy_wave) {
    if (amy_wave > 8) return SmkWaveType::Sine;
    return static_cast<SmkWaveType>(amy_wave);
}

inline const char* waveTypeName(SmkWaveType type) {
    switch (type) {
        case SmkWaveType::Sine:          return "SINE";
        case SmkWaveType::Pulse:         return "PULSE";
        case SmkWaveType::SawDown:       return "SAW DOWN";
        case SmkWaveType::SawUp:         return "SAW UP";
        case SmkWaveType::Triangle:      return "TRIANGLE";
        case SmkWaveType::Noise:         return "NOISE";
        case SmkWaveType::KarplusStrong: return "KARPLUS";
        case SmkWaveType::Pcm:           return "PCM";
        case SmkWaveType::Algo:          return "FM ALGO";
        default:                         return "UNKNOWN";
    }
}

inline uint8_t mapLegacyWaveToAmy(uint8_t legacy_wave) {
    switch (legacy_wave) {
        case 1: return toAmyWaveType(SmkWaveType::SawDown);   // Legacy 1: SawDown -> AMY 2
        case 2: return toAmyWaveType(SmkWaveType::SawUp);     // Legacy 2: SawUp -> AMY 3
        case 3: return toAmyWaveType(SmkWaveType::Triangle);  // Legacy 3: Triangle -> AMY 4
        case 4: return toAmyWaveType(SmkWaveType::Pulse);     // Legacy 4: Pulse -> AMY 1
        default: return legacy_wave;                          // 0: Sine -> AMY 0, etc.
    }
}

struct SynthPatchV3 {
    uint8_t     id;
    char        name[24];
    char        category[16];
    char        author[16];
    uint16_t    engine_patch; // AMY preset or patch ID (0..127 Juno, 128..255 DX7, 256+ PCM)
    int8_t      transpose;    // Transpose in semitones (-24..+24)
    uint8_t     voice_count;  // Max polyphony voices (e.g. 8)
    uint8_t     wave_type;    // AMY wave: 0=SINE, 1=PULSE, 2=SAW_DOWN, 3=SAW_UP, 4=TRIANGLE, 5=NOISE, 6=KS, 7=PCM, 8=ALGO
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

// Legacy format_version == 4. Two short-lived semantics reused this version:
//   1. The first Expansion v4 (officially supported here): legacy wave/filter/
//      chorus/drive enums whose *effective* firmware behavior is migrated.
//   2. A brief stabilization window that also wrote format_version == 4 but
//      already used native AMY wave/filter enums and 0..1 drive.
// Both wrote the same struct layout and never populated reserved[] distinctly,
// so data_size, range checks and header metadata cannot deterministically tell
// them apart. We therefore support interpretation (1) only and do not guess.
// See docs/patch_v4_compatibility.md.
struct SynthPatchV4Legacy {
    uint8_t     id;
    char        name[24];
    char        category[16];
    char        author[16];
    uint16_t    engine_patch; // AMY preset or patch ID (0..127 Juno, 128..255 DX7, 256+ PCM)
    int8_t      transpose;    // Transpose in semitones (-24..+24)
    uint8_t     voice_count;  // Max polyphony voices (e.g. 8)
    uint8_t     wave_type;    // Legacy 0=Sine, 1=SawDown, 2=SawUp, 3=Triangle, 4=Square, 5=Noise, 6=KS, 7=PCM, 8=ALGO
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
    // Legacy v4 fields:
    float       filter_env_amount;   // Envelope amount to filter cutoff (-4.0 to +4.0)
    float       filter_key_tracking; // Filter keyboard tracking (0.0 to 2.0)
    float       filter_vel_tracking; // Filter velocity tracking (0.0 to 2.0)
    uint8_t     filter_type;         // Legacy realized behavior: 0=Inherit, 1=LPF, 2=BPF, 3=HPF (documented enum never matched DSP)
    float       osc_mix;             // Sub/main osc mix (0.0 to 1.0)
    float       osc_detune;          // Detune in cents (-100.0 to +100.0)
    float       sub_level;           // Sub-oscillator level (0.0 to 1.0)
    float       noise_level;         // Noise level (0.0 to 1.0)
    float       drive_level;         // Saturation / drive level (0.0 to 3.0)
    float       master_tone;         // Tilt EQ tone (-1.0 to +1.0)
    uint8_t     chorus_mode;         // Legacy: 0=Classic, 1=Juno, 2=Ensemble, 3=Wide, 4=Vibrato
    uint8_t     reverb_freeze;       // 0=Normal, 1=Frozen reverb tank
    uint8_t     reserved[6];         // Reserved padding
    uint32_t    crc32;
};

struct SynthPatch {
    uint8_t     id;
    char        name[24];
    char        category[16];
    char        author[16];
    uint16_t    engine_patch; // AMY preset or patch ID (0..127 Juno, 128..255 DX7, 256+ PCM)
    int8_t      transpose;    // Transpose in semitones (-24..+24)
    uint8_t     voice_count;  // Max polyphony voices (e.g. 8)
    uint8_t     wave_type;    // AMY wave: 0=SINE, 1=PULSE, 2=SAW_DOWN, 3=SAW_UP, 4=TRIANGLE, 5=NOISE, 6=KS, 7=PCM, 8=ALGO
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
    // v5 fields:
    float       filter_env_amount;   // Envelope amount to filter cutoff (-4.0 to +4.0)
    float       filter_key_tracking; // Filter keyboard tracking (0.0 to 2.0)
    float       filter_vel_tracking; // Filter velocity tracking (0.0 to 2.0)
    uint8_t     filter_type;         // SmkFilterType: 0xFF=Inherit, 0=None, 1=LPF, 2=BPF, 3=HPF, 4=LPF24, 5=Notch, 6=Phaser, 7=Moog24, 8=TptSvf
    float       osc_mix;             // Sub/main osc mix (0.0 to 1.0)
    float       osc_detune;          // Detune in cents (-100.0 to +100.0)
    float       sub_level;           // Sub-oscillator level (0.0 to 1.0)
    float       noise_level;         // Noise level (0.0 to 1.0)
    float       drive_level;         // Saturation / drive level normalized (0.0 to 1.0, post-FX bus)
    float       master_tone;         // Tilt EQ tone (-1.0 to +1.0)
    uint8_t     chorus_mode;         // 0=Off, 1=Classic, 2=Juno, 3=Ensemble, 4=Wide, 5=Vibrato
    uint8_t     reverb_freeze;       // 0=Normal, 1=Frozen reverb tank
    uint8_t     reserved[6];         // Reserved padding
    uint32_t    crc32;
};

// Helper functions to calculate CRC32 checksums
uint32_t calculatePatchCrc32(const SynthPatch& patch);
uint32_t calculatePatchV3Crc32(const SynthPatchV3& patch);
uint32_t calculatePatchV4LegacyCrc32(const SynthPatchV4Legacy& patch);

} // namespace smk
