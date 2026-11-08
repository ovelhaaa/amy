#include "ui_parameters.h"
#include <cstdio>
#include <cstring>

namespace smk::p4::ui {

namespace {

#if defined(SMKP4_SOUND_EDITOR) || defined(CONFIG_SMKP4_SOUND_EDITOR)
constexpr float kResonanceMaximum=2.0f;
#else
constexpr float kResonanceMaximum=8.0f;
#endif
constexpr const char* kLayerModes[] = {"SINGLE A", "SINGLE B", "DUAL LAYER", "SPLIT A/B"};
constexpr const char* kWaveforms[] = {"SINE", "PULSE", "SAW DOWN", "SAW UP", "TRIANGLE", "NOISE", "KARPLUS", "PCM", "FM ALGO"};
constexpr const char* kFilterTypes[] = {"PRESET", "NONE", "LPF 12dB", "BPF", "HPF", "LPF 24dB", "NOTCH", "PHASER", "MOOG 24", "TPT SVF"};
constexpr const char* kEnvCurves[] = {"LINEAR", "EXP", "LOG", "S-CURVE"};
constexpr const char* kLfoWaves[] = {"SINE", "TRIANGLE", "SAW", "SQUARE", "S&H"};
constexpr const char* kLfoDests[] = {"NONE", "PITCH", "CUTOFF", "AMP", "PWM", "PAN", "FM MOD"};
constexpr const char* kModSources[] = {"OFF", "MOD WHEEL", "VELOCITY", "AFTERTOUCH", "LFO 1", "LFO 2", "ENV 2", "MACRO 1", "MACRO 2", "KEYTRACK"};
constexpr const char* kModDests[] = {"NONE", "OSC PITCH", "OSC PWM", "FILTER CUT", "FILTER RES", "AMP LEVEL", "PAN", "LFO RATE", "FM INDEX", "FX SEND"};
constexpr const char* kModPolarities[] = {"UNIPOLAR +", "BIPOLAR +/-", "INVERTED -"};
constexpr const char* kChorusModes[] = {"OFF", "CLASSIC", "JUNO I", "JUNO II", "ENSEMBLE", "VIBRATO"};
constexpr const char* kFxRouting[] = {"CHOR > DLY > REV", "DLY > CHOR > REV", "PARALLEL SEND", "BYPASS ALL"};
constexpr const char* kClockSources[] = {"INTERNAL", "USB MIDI", "AUTO"};
constexpr const char* kArpRates[] = {"1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32"};
constexpr const char* kArpDirs[] = {"UP", "DOWN", "UP/DOWN", "RANDOM", "AS PLAYED", "CHORD"};
constexpr const char* kDrumVoices[] = {"808 KICK", "808 SNARE", "CLOSED HAT", "OPEN HAT", "LOW TOM", "MID TOM", "CLAP", "RIMSHOT"};
constexpr const char* kRootNotes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
constexpr const char* kScaleTypes[] = {
    "CHROMATIC", "MAJOR", "NAT MINOR", "HARM MINOR", "PENTA MAJ",
    "PENTA MIN", "BLUES", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYDIAN"
};
constexpr const char* kQuantizeModes[] = {"NEAR UP", "NEAR DOWN", "SNAP ROOT", "FILTER OUT", "DOWN", "UP"};
constexpr const char* kChordTypes[] = {
    "OFF", "MAJOR", "MINOR", "DIMINISHED", "AUGMENTED",
    "SUS2", "SUS4", "DOM 7", "MAJ 7", "MIN 7", "ADD9 (PLAN)", "9TH (PLAN)", "DIAT TRIAD", "DIAT 7"
};
constexpr const char* kChordVoicings[] = {"CLOSE", "OPEN", "DROP 2", "SPREAD", "POWER"};
constexpr const char* kHarmonyPresets[]={"NORMAL KEYBOARD","C MAJ TRIADS","A MIN TRIADS","C MAJ SEVENTHS","SCALE ONLY"};
constexpr const char* kVelBehaviors[] = {"FOLLOW IN", "FIXED 100", "HUMANIZE", "ACCENT"};
constexpr const char* kDestLayers[] = {"LAYER A", "LAYER B", "BOTH A+B"};
constexpr const char* kVelCurves[] = {"LINEAR", "SOFT", "HARD", "FIXED 100"};
constexpr const char* kThemes[] = {"INDUSTRIAL", "HI-CONTRAST", "AMBER"};
constexpr const char* kBootModes[] = {"LAST PATCH", "FACTORY 128", "SAFE SILENT"};

constexpr uint16_t kStdCtrl = kCapControllable | kCapMidiMappable | kCapPersistedInPatch;
constexpr uint16_t kModTargetCtrl = kStdCtrl | kCapAutomatable | kCapMacroTargetable;

const ParameterDescriptor kDescriptors[] = {
    // ── PERFORMANCE & MASTER ─────────────────────────────────────────────
    {"perf.patch_id", "perf", "PATCH ID", ParameterValueType::Int, 0.0f, 255.0f, 128.0f, 1.0f, "",
     ParameterScale::Linear, "Performance", "PATCH", nullptr, 0, kCapControllable | kCapPersistedInPatch,
     FeatureStatus::Available, nullptr, -1, "smk-p4/components/amy_runtime/runtime_config.h",
     "Active patch ID (ROM 128/135 available in P4 standalone; broader library in S3/Demo)"},
    {"perf.master_volume", "perf", "MASTER VOL", ParameterValueType::Int, 0.0f, 127.0f, 64.0f, 1.0f, "",
     ParameterScale::Linear, "Performance", "MASTER", nullptr, 0, kStdCtrl,
     FeatureStatus::Available, nullptr, -1, "smk-p4/components/amy_runtime/amy_runtime.cpp",
     "Master output level with per-frame smoothing"},
    {"perf.bpm", "perf", "TEMPO BPM", ParameterValueType::Float, 40.0f, 240.0f, 120.0f, 0.5f, "BPM",
     ParameterScale::Linear, "Performance", "TRANSPORT", nullptr, 0, kCapControllable | kCapPersistedInPatch,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/sequencer/include/clock_manager.h",
     "Global master clock tempo in BPM"},
    {"perf.octave_shift", "perf", "OCTAVE", ParameterValueType::Int, -2.0f, 2.0f, 0.0f, 1.0f, "oct",
     ParameterScale::Linear, "Performance", "KEYBOARD", nullptr, 0, kCapControllable,
     FeatureStatus::Available, nullptr, -1, "smk-p4/main/performance_ui.cpp",
     "Touch keyboard octave shift"},
    {"perf.layer_mode", "perf", "LAYER MODE", ParameterValueType::Enum, 0.0f, 3.0f, 0.0f, 1.0f, "",
     ParameterScale::Linear, "Performance", "LAYERS", kLayerModes, 4, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/audio/include/performance.h",
     "Active keyboard performance mode (Single A, Single B, Dual Layer, Split A/B)"},
    {"perf.macro.1", "perf", "M1 CHAR", ParameterValueType::Int, 0.0f, 127.0f, 64.0f, 1.0f, "",
     ParameterScale::Linear, "Performance", "MACROS", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/macro_profile.h",
     "Contextual Macro 1 (Character / FM Mod Index)"},
    {"perf.macro.2", "perf", "M2 BRIGHT", ParameterValueType::Int, 0.0f, 127.0f, 64.0f, 1.0f, "",
     ParameterScale::Linear, "Performance", "MACROS", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/macro_profile.h",
     "Contextual Macro 2 (Brightness / Tone)"},
    {"perf.macro.3", "perf", "M3 MOTION", ParameterValueType::Int, 0.0f, 127.0f, 24.0f, 1.0f, "",
     ParameterScale::Linear, "Performance", "MACROS", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/macro_profile.h",
     "Contextual Macro 3 (Motion / Chorus / Detune)"},
    {"perf.macro.4", "perf", "M4 SHAPE", ParameterValueType::Int, 0.0f, 127.0f, 64.0f, 1.0f, "",
     ParameterScale::Linear, "Performance", "MACROS", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/macro_profile.h",
     "Contextual Macro 4 (Envelope Shape / FM Feedback)"},
    {"perf.macro.5", "perf", "M5 ATTACK", ParameterValueType::Int, 0.0f, 127.0f, 32.0f, 1.0f, "",
     ParameterScale::Linear, "Performance", "MACROS", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/macro_profile.h",
     "Contextual Macro 5 (Attack / FM Ratio)"},
    {"perf.macro.6", "perf", "M6 RELEASE", ParameterValueType::Int, 0.0f, 127.0f, 52.0f, 1.0f, "",
     ParameterScale::Linear, "Performance", "MACROS", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/macro_profile.h",
     "Contextual Macro 6 (Release / FM Detune)"},
    {"perf.macro.7", "perf", "M7 SPACE", ParameterValueType::Int, 0.0f, 127.0f, 36.0f, 1.0f, "",
     ParameterScale::Linear, "Performance", "MACROS", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/macro_profile.h",
     "Contextual Macro 7 (Space / Reverb & Delay Send)"},
    {"perf.macro.8", "perf", "M8 DRIVE", ParameterValueType::Int, 0.0f, 127.0f, 16.0f, 1.0f, "",
     ParameterScale::Linear, "Performance", "MACROS", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/macro_profile.h",
     "Contextual Macro 8 (Drive / Edge)"},

    // ── SOUND EDITOR: OSC ────────────────────────────────────────────────
    {"osc.1.wave", "osc", "OSC 1 WAVE", ParameterValueType::Enum, 0.0f, 8.0f, 8.0f, 1.0f, "",
     ParameterScale::Linear, "Synthesis", "OSC", kWaveforms, 9, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/patch_types.h",
     "Primary oscillator waveform / synthesis family"},
    {"osc.1.pitch_st", "osc", "COARSE PITCH", ParameterValueType::Int, -24.0f, 24.0f, 0.0f, 1.0f, "st",
     ParameterScale::Linear, "Synthesis", "OSC", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/patch_types.h",
     "Oscillator coarse pitch transpose in semitones"},
    {"osc.1.detune_ct", "osc", "FINE DETUNE", ParameterValueType::Float, -100.0f, 100.0f, 0.0f, 1.0f, "ct",
     ParameterScale::Linear, "Synthesis", "OSC", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/patch_types.h",
     "Oscillator fine detune in cents"},
    {"osc.1.level", "osc", "OSC LEVEL", ParameterValueType::Float, 0.0f, 100.0f, 85.0f, 1.0f, "%",
     ParameterScale::Linear, "Synthesis", "OSC", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/patch_types.h",
     "Primary oscillator mix level"},
    {"osc.1.pan", "osc", "OSC PAN", ParameterValueType::Int, -64.0f, 63.0f, 0.0f, 1.0f, "",
     ParameterScale::Linear, "Synthesis", "OSC", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Planned, nullptr, -1, "src/amy.h",
     "Oscillator stereo pan position"},
    {"osc.1.unison", "osc", "UNISON VOICES", ParameterValueType::Int, 1.0f, 4.0f, 1.0f, 1.0f, "vx",
     ParameterScale::Linear, "Synthesis", "OSC", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "specs.md",
     "Unison stacked voices per note"},
    {"osc.1.pulse_width", "osc", "PULSE WIDTH", ParameterValueType::Float, 5.0f, 95.0f, 50.0f, 1.0f, "%",
     ParameterScale::Linear, "Synthesis", "OSC", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Planned, "osc.1.wave", 1, "src/amy.h",
     "Pulse width duty cycle (active only when OSC 1 WAVE is PULSE)"},
    {"osc.sub_level", "osc", "SUB OSC", ParameterValueType::Float, 0.0f, 100.0f, 20.0f, 1.0f, "%",
     ParameterScale::Linear, "Synthesis", "OSC", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/patch_types.h",
     "Sub-oscillator level (-1 octave)"},
    {"osc.noise_level", "osc", "NOISE MIX", ParameterValueType::Float, 0.0f, 100.0f, 0.0f, 1.0f, "%",
     ParameterScale::Linear, "Synthesis", "OSC", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/patch_types.h",
     "White/pink noise generator level"},

    // ── SOUND EDITOR: FM ─────────────────────────────────────────────────
    {"fm.depth", "fm", "FM BRIGHTNESS", ParameterValueType::Float, 0.0f, 1.0f, 1.0f, 0.01f, "x",
     ParameterScale::Linear, "Synthesis", "FM", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Planned, nullptr, -1, "src/algorithms.c",
     "Relative modulation intensity; native routing determines modulators"},
    {"fm.algo", "fm", "FM ALGORITHM", ParameterValueType::Int, 1.0f, 32.0f, 1.0f, 1.0f, "",
     ParameterScale::Linear, "Synthesis", "FM", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, "osc.1.wave", 8, "smk-s3/components/synth/include/macro_profile.h",
     "DX7 6-operator routing algorithm (1..32)"},
    {"fm.selected_op", "fm", "ACTIVE OP", ParameterValueType::Int, 1.0f, 6.0f, 1.0f, 1.0f, "",
     ParameterScale::Linear, "Synthesis", "FM", nullptr, 0, kCapControllable,
     FeatureStatus::Demo, "osc.1.wave", 8, "src/algorithms.c",
     "Selected FM operator for detailed editing (OP1..OP6)"},
    {"fm.op.ratio", "fm", "OP FREQ RATIO", ParameterValueType::Float, 0.5f, 16.0f, 1.0f, 0.25f, "x",
     ParameterScale::Linear, "Synthesis", "FM", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Planned, "osc.1.wave", 8, "smk-s3/components/synth/include/macro_profile.h",
     "Selected FM operator frequency multiplier ratio"},
    {"fm.op.level", "fm", "OP OUT LEVEL", ParameterValueType::Float, 0.0f, 100.0f, 80.0f, 1.0f, "%",
     ParameterScale::Linear, "Synthesis", "FM", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Planned, "osc.1.wave", 8, "smk-s3/components/synth/include/macro_profile.h",
     "Selected FM operator modulation/carrier output level"},
    {"fm.feedback", "fm", "FM FEEDBACK", ParameterValueType::Float, 0.0f, 100.0f, 25.0f, 1.0f, "%",
     ParameterScale::Linear, "Synthesis", "FM", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Planned, "osc.1.wave", 8, "smk-s3/components/synth/include/macro_profile.h",
     "Operator feedback loop intensity"},
    {"fm.op.attack", "fm", "OP ENV ATK", ParameterValueType::Float, 1.0f, 2000.0f, 15.0f, 5.0f, "ms",
     ParameterScale::Logarithmic, "Synthesis", "FM", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, "osc.1.wave", 8, "src/envelope.c",
     "Per-operator envelope attack time"},
    {"fm.op.decay", "fm", "OP ENV DEC", ParameterValueType::Float, 10.0f, 4000.0f, 350.0f, 10.0f, "ms",
     ParameterScale::Logarithmic, "Synthesis", "FM", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, "osc.1.wave", 8, "src/envelope.c",
     "Per-operator envelope decay time"},
    {"fm.op.vel_sens", "fm", "OP VEL SENS", ParameterValueType::Float, 0.0f, 100.0f, 60.0f, 1.0f, "%",
     ParameterScale::Linear, "Synthesis", "FM", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, "osc.1.wave", 8, "src/patches.c",
     "Operator velocity sensitivity"},

    // ── SOUND EDITOR: FILTER ─────────────────────────────────────────────
    {"filter.type", "filter", "FILTER TYPE", ParameterValueType::Enum, 0.0f, 9.0f, 2.0f, 1.0f, "",
     ParameterScale::Linear, "Synthesis", "FILTER", kFilterTypes, 10, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/patch_types.h",
     "Filter topology (LPF12, BPF, HPF, LPF24, Notch, Phaser, Moog24, SVF)"},
    {"filter.cutoff_hz", "filter", "CUTOFF FREQ", ParameterValueType::Float, 20.0f, 18000.0f, 4200.0f, 20.0f, "Hz",
     ParameterScale::Logarithmic, "Synthesis", "FILTER", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/patch_types.h",
     "Filter cutoff frequency in Hz"},
    {"filter.resonance", "filter", "RESONANCE", ParameterValueType::Float, 0.1f, kResonanceMaximum, 1.2f, 0.1f, "Q",
     ParameterScale::Linear, "Synthesis", "FILTER", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/patch_types.h",
     "Filter resonance Q factor"},
    {"filter.drive", "filter", "FILTER DRIVE", ParameterValueType::Float, 0.0f, 100.0f, 15.0f, 1.0f, "%",
     ParameterScale::Linear, "Synthesis", "FILTER", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/patch_types.h",
     "Pre-filter/post-filter nonlinear saturation"},
    {"filter.keytrack", "filter", "KEY TRACKING", ParameterValueType::Float, 0.0f, 200.0f, 100.0f, 5.0f, "%",
     ParameterScale::Linear, "Synthesis", "FILTER", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/patch_types.h",
     "Filter keyboard pitch tracking percentage"},
    {"filter.env_amount", "filter", "ENV AMOUNT", ParameterValueType::Float, -4.0f, 4.0f, 1.5f, 0.1f, "oct",
     ParameterScale::Linear, "Synthesis", "FILTER", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/patch_types.h",
     "Bipolar filter envelope depth in octaves"},

    // ── SOUND EDITOR: AMP ────────────────────────────────────────────────
    {"amp.level", "amp", "PATCH LEVEL", ParameterValueType::Float, 0.0f, 100.0f, 80.0f, 1.0f, "%",
     ParameterScale::Linear, "Synthesis", "AMP", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Planned, nullptr, -1, "src/amy.h",
     "Voice amplifier output gain"},
    {"amp.pan", "amp", "VOICE PAN", ParameterValueType::Int, -64.0f, 63.0f, 0.0f, 1.0f, "",
     ParameterScale::Linear, "Synthesis", "AMP", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Planned, nullptr, -1, "src/amy.h",
     "Voice stereo pan position"},
    {"amp.vel_sens", "amp", "VELOCITY SENS", ParameterValueType::Float, 0.0f, 100.0f, 85.0f, 1.0f, "%",
     ParameterScale::Linear, "Synthesis", "AMP", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-p4/components/amy_runtime/amy_runtime.cpp",
     "Note-on velocity sensitivity to voice amplitude"},
    {"amp.portamento_ms", "amp", "PORTAMENTO", ParameterValueType::Int, 0.0f, 2000.0f, 0.0f, 10.0f, "ms",
     ParameterScale::Logarithmic, "Synthesis", "AMP", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/patch_types.h",
     "Pitch glide time between notes in ms"},

    // ── SOUND EDITOR: ENV ────────────────────────────────────────────────
    {"env.amp.attack_ms", "env", "ATTACK", ParameterValueType::Float, 1.0f, 5000.0f, 15.0f, 5.0f, "ms",
     ParameterScale::Logarithmic, "Synthesis", "ENV", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/patch_types.h",
     "Amplitude ADSR attack time"},
    {"env.amp.decay_ms", "env", "DECAY", ParameterValueType::Float, 1.0f, 30000.0f, 350.0f, 10.0f, "ms",
     ParameterScale::Logarithmic, "Synthesis", "ENV", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/patch_types.h",
     "Amplitude ADSR decay time"},
    {"env.amp.sustain", "env", "SUSTAIN", ParameterValueType::Float, 0.0f, 100.0f, 75.0f, 1.0f, "%",
     ParameterScale::Linear, "Synthesis", "ENV", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/patch_types.h",
     "Amplitude ADSR sustain level"},
    {"env.amp.release_ms", "env", "RELEASE", ParameterValueType::Float, 5.0f, 8000.0f, 280.0f, 10.0f, "ms",
     ParameterScale::Logarithmic, "Synthesis", "ENV", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/patch_types.h",
     "Amplitude ADSR release time"},
    {"env.amp.curve", "env", "CURVE", ParameterValueType::Enum, 0.0f, 3.0f, 1.0f, 1.0f, "",
     ParameterScale::Linear, "Synthesis", "ENV", kEnvCurves, 4, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "src/envelope.c",
     "Envelope segment curvature law"},

    // ── SOUND EDITOR: LFO ────────────────────────────────────────────────
    {"lfo.1.wave", "lfo", "LFO WAVE", ParameterValueType::Enum, 0.0f, 4.0f, 0.0f, 1.0f, "",
     ParameterScale::Linear, "Synthesis", "LFO", kLfoWaves, 5, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "src/amy.h",
     "Low-frequency oscillator waveform"},
    {"lfo.1.rate_hz", "lfo", "LFO RATE", ParameterValueType::Float, 0.1f, 30.0f, 5.0f, 0.1f, "Hz",
     ParameterScale::Logarithmic, "Synthesis", "LFO", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Available, nullptr, -1, "smk-p4/components/amy_runtime/amy_runtime.cpp",
     "LFO frequency (5 Hz vibrato active via CC1 in P4 standalone)"},
    {"lfo.1.depth", "lfo", "LFO DEPTH", ParameterValueType::Float, 0.0f, 100.0f, 25.0f, 1.0f, "%",
     ParameterScale::Linear, "Synthesis", "LFO", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Available, nullptr, -1, "smk-p4/components/amy_runtime/amy_runtime.cpp",
     "LFO modulation depth"},
    {"lfo.1.phase_deg", "lfo", "LFO PHASE", ParameterValueType::Int, 0.0f, 360.0f, 0.0f, 15.0f, "deg",
     ParameterScale::Linear, "Synthesis", "LFO", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "src/amy.h",
     "LFO trigger initial phase"},
    {"lfo.1.sync", "lfo", "TEMPO SYNC", ParameterValueType::Bool, 0.0f, 1.0f, 0.0f, 1.0f, "",
     ParameterScale::Linear, "Synthesis", "LFO", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "specs.md",
     "Sync LFO rate to master clock BPM"},
    {"lfo.1.dest", "lfo", "LFO TARGET", ParameterValueType::Enum, 0.0f, 6.0f, 1.0f, 1.0f, "",
     ParameterScale::Linear, "Synthesis", "LFO", kLfoDests, 7, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "src/amy.h",
     "Primary LFO destination parameter"},

    // ── MODULATION MATRIX ────────────────────────────────────────────────
    {"mod.slot1.source", "mod", "SLOT 1 SRC", ParameterValueType::Enum, 0.0f, 9.0f, 1.0f, 1.0f, "",
     ParameterScale::Linear, "Modulation", "MATRIX", kModSources, 10, kStdCtrl,
     FeatureStatus::Available, nullptr, -1, "smk-p4/components/amy_runtime/amy_runtime.cpp",
     "Modulation slot 1 source (Mod Wheel CC1 active in P4)"},
    {"mod.slot1.dest", "mod", "SLOT 1 DEST", ParameterValueType::Enum, 0.0f, 9.0f, 1.0f, 1.0f, "",
     ParameterScale::Linear, "Modulation", "MATRIX", kModDests, 10, kStdCtrl,
     FeatureStatus::Available, nullptr, -1, "smk-p4/components/amy_runtime/amy_runtime.cpp",
     "Modulation slot 1 destination (OSC Pitch Vibrato active in P4)"},
    {"mod.slot1.amount", "mod", "SLOT 1 AMT", ParameterValueType::Float, -100.0f, 100.0f, 40.0f, 1.0f, "%",
     ParameterScale::Linear, "Modulation", "MATRIX", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "specs.md",
     "Modulation slot 1 depth percentage"},
    {"mod.slot1.polarity", "mod", "SLOT 1 POL", ParameterValueType::Enum, 0.0f, 2.0f, 1.0f, 1.0f, "",
     ParameterScale::Linear, "Modulation", "MATRIX", kModPolarities, 3, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/macro_profile.h",
     "Modulation slot 1 polarity law"},

    // ── LAYERS & SPLIT ───────────────────────────────────────────────────
    {"layer_a.enable", "layer_a", "ON", ParameterValueType::Bool, 0.0f, 1.0f, 1.0f, 1.0f, "",
     ParameterScale::Linear, "Layers", "LAYER A", nullptr, 0, kStdCtrl | kCapLayerScoped,
     FeatureStatus::Available, nullptr, -1, "smk-p4/components/amy_runtime/amy_runtime.cpp",
     "Enable primary Layer A synth voice allocation"},
    {"layer_a.patch_id", "layer_a", "PATCH", ParameterValueType::Int, 0.0f, 255.0f, 128.0f, 1.0f, "",
     ParameterScale::Linear, "Layers", "LAYER A", nullptr, 0, kStdCtrl | kCapLayerScoped,
     FeatureStatus::Available, nullptr, -1, "smk-p4/components/amy_runtime/runtime_config.h",
     "Patch assigned to Layer A"},
    {"layer_a.channel", "layer_a", "CH A", ParameterValueType::Int, 1.0f, 16.0f, 1.0f, 1.0f, "ch",
     ParameterScale::Linear, "Layers", "LAYER A", nullptr, 0, kStdCtrl | kCapLayerScoped,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/audio/include/performance.h",
     "MIDI input channel for Layer A"},
    {"layer_a.octave", "layer_a", "OCT", ParameterValueType::Int, -3.0f, 3.0f, 0.0f, 1.0f, "oct",
     ParameterScale::Linear, "Layers", "LAYER A", nullptr, 0, kStdCtrl | kCapLayerScoped,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/audio/include/performance.h",
     "Octave shift for Layer A"},
    {"layer_a.transpose", "layer_a", "SEMI", ParameterValueType::Int, -12.0f, 12.0f, 0.0f, 1.0f, "st",
     ParameterScale::Linear, "Layers", "LAYER A", nullptr, 0, kStdCtrl | kCapLayerScoped,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/audio/include/performance.h",
     "Semitone transpose for Layer A"},
    {"layer_a.key_low", "layer_a", "LOW", ParameterValueType::Int, 0.0f, 127.0f, 36.0f, 1.0f, "",
     ParameterScale::Linear, "Layers", "LAYER A", nullptr, 0, kStdCtrl | kCapLayerScoped,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/audio/include/note_routing.h",
     "Lowest MIDI note playable by Layer A"},
    {"layer_a.key_high", "layer_a", "HIGH", ParameterValueType::Int, 0.0f, 127.0f, 59.0f, 1.0f, "",
     ParameterScale::Linear, "Layers", "LAYER A", nullptr, 0, kStdCtrl | kCapLayerScoped,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/audio/include/note_routing.h",
     "Highest MIDI note playable by Layer A"},
    {"layer_b.enable", "layer_b", "ON", ParameterValueType::Bool, 0.0f, 1.0f, 1.0f, 1.0f, "",
     ParameterScale::Linear, "Layers", "LAYER B", nullptr, 0, kStdCtrl | kCapLayerScoped,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/audio/include/performance.h",
     "Enable secondary Layer B (implemented in S3 dual runtime; planned on P4)"},
    {"layer_b.patch_id", "layer_b", "PATCH", ParameterValueType::Int, 0.0f, 255.0f, 135.0f, 1.0f, "",
     ParameterScale::Linear, "Layers", "LAYER B", nullptr, 0, kStdCtrl | kCapLayerScoped,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/audio/include/performance.h",
     "Patch assigned to Layer B"},
    {"layer_b.channel", "layer_b", "CH B", ParameterValueType::Int, 1.0f, 16.0f, 2.0f, 1.0f, "ch",
     ParameterScale::Linear, "Layers", "LAYER B", nullptr, 0, kStdCtrl | kCapLayerScoped,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/audio/include/performance.h",
     "MIDI input channel for Layer B"},
    {"layer_b.octave", "layer_b", "OCT", ParameterValueType::Int, -3.0f, 3.0f, 0.0f, 1.0f, "oct",
     ParameterScale::Linear, "Layers", "LAYER B", nullptr, 0, kStdCtrl | kCapLayerScoped,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/audio/include/performance.h",
     "Octave shift for Layer B"},
    {"layer_b.transpose", "layer_b", "SEMI", ParameterValueType::Int, -12.0f, 12.0f, 0.0f, 1.0f, "st",
     ParameterScale::Linear, "Layers", "LAYER B", nullptr, 0, kStdCtrl | kCapLayerScoped,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/audio/include/performance.h",
     "Semitone transpose for Layer B"},
    {"layer_b.key_low", "layer_b", "LOW", ParameterValueType::Int, 0.0f, 127.0f, 60.0f, 1.0f, "",
     ParameterScale::Linear, "Layers", "LAYER B", nullptr, 0, kStdCtrl | kCapLayerScoped,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/audio/include/note_routing.h",
     "Lowest MIDI note playable by Layer B"},
    {"layer_b.key_high", "layer_b", "HIGH", ParameterValueType::Int, 0.0f, 127.0f, 96.0f, 1.0f, "",
     ParameterScale::Linear, "Layers", "LAYER B", nullptr, 0, kStdCtrl | kCapLayerScoped,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/audio/include/note_routing.h",
     "Highest MIDI note playable by Layer B"},

    // ── MIXER ────────────────────────────────────────────────────────────
    {"mixer.layer_a.level", "mixer", "LAYER A LVL", ParameterValueType::Int, 0.0f, 100.0f, 85.0f, 1.0f, "%",
     ParameterScale::Linear, "Mixer", "CHANNELS", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/audio/include/performance.h",
     "Layer A mixer fader level"},
    {"mixer.layer_a.pan", "mixer", "LAYER A PAN", ParameterValueType::Int, -64.0f, 63.0f, -10.0f, 1.0f, "",
     ParameterScale::Linear, "Mixer", "CHANNELS", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/audio/include/performance.h",
     "Layer A stereo pan"},
    {"mixer.layer_a.mute", "mixer", "LAYER A MUTE", ParameterValueType::Bool, 0.0f, 1.0f, 0.0f, 1.0f, "",
     ParameterScale::Linear, "Mixer", "CHANNELS", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/audio/include/performance.h",
     "Layer A channel mute"},
    {"mixer.layer_b.level", "mixer", "LAYER B LVL", ParameterValueType::Int, 0.0f, 100.0f, 75.0f, 1.0f, "%",
     ParameterScale::Linear, "Mixer", "CHANNELS", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/audio/include/performance.h",
     "Layer B mixer fader level"},
    {"mixer.layer_b.pan", "mixer", "LAYER B PAN", ParameterValueType::Int, -64.0f, 63.0f, 12.0f, 1.0f, "",
     ParameterScale::Linear, "Mixer", "CHANNELS", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/audio/include/performance.h",
     "Layer B stereo pan"},
    {"mixer.layer_b.mute", "mixer", "LAYER B MUTE", ParameterValueType::Bool, 0.0f, 1.0f, 0.0f, 1.0f, "",
     ParameterScale::Linear, "Mixer", "CHANNELS", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/audio/include/performance.h",
     "Layer B channel mute"},
    {"mixer.drums.level", "mixer", "DRUMS LVL", ParameterValueType::Int, 0.0f, 100.0f, 80.0f, 1.0f, "%",
     ParameterScale::Linear, "Mixer", "CHANNELS", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/pad_bank.h",
     "Drums bus mixer fader level"},
    {"mixer.drums.mute", "mixer", "DRUMS MUTE", ParameterValueType::Bool, 0.0f, 1.0f, 0.0f, 1.0f, "",
     ParameterScale::Linear, "Mixer", "CHANNELS", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/pad_bank.h",
     "Drums bus mute"},
    {"mixer.fx_send", "mixer", "BUS FX SEND", ParameterValueType::Int, 0.0f, 100.0f, 35.0f, 1.0f, "%",
     ParameterScale::Linear, "Mixer", "CHANNELS", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/patch_types.h",
     "Global auxiliary effects send level"},

    // ── EFFECTS ──────────────────────────────────────────────────────────
    {"fx.chorus.mode", "fx", "CHORUS MODE", ParameterValueType::Enum, 0.0f, 5.0f, 2.0f, 1.0f, "",
     ParameterScale::Linear, "Effects", "CHORUS", kChorusModes, 6, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/patch_types.h",
     "AMY stereo chorus mode (supported in AMY core; off in P4 0.3 budget)"},
    {"fx.chorus.depth", "fx", "CHORUS DEPTH", ParameterValueType::Float, 0.0f, 100.0f, 45.0f, 1.0f, "%",
     ParameterScale::Linear, "Effects", "CHORUS", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/patch_types.h",
     "Stereo chorus modulation depth"},
    {"fx.delay.time_ms", "fx", "DELAY TIME", ParameterValueType::Float, 10.0f, 1000.0f, 350.0f, 10.0f, "ms",
     ParameterScale::Linear, "Effects", "DELAY", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/patch_types.h",
     "Echo/delay time in ms (AMY echo module)"},
    {"fx.delay.feedback", "fx", "DELAY FDBK", ParameterValueType::Float, 0.0f, 95.0f, 40.0f, 1.0f, "%",
     ParameterScale::Linear, "Effects", "DELAY", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/patch_types.h",
     "Delay regeneration feedback"},
    {"fx.delay.mix", "fx", "DELAY MIX", ParameterValueType::Float, 0.0f, 100.0f, 25.0f, 1.0f, "%",
     ParameterScale::Linear, "Effects", "DELAY", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/patch_types.h",
     "Delay wet/dry mix"},
    {"fx.reverb.size", "fx", "REVERB SIZE", ParameterValueType::Float, 0.0f, 100.0f, 70.0f, 1.0f, "%",
     ParameterScale::Linear, "Effects", "REVERB", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/patch_types.h",
     "Stereo reverb room size / decay"},
    {"fx.reverb.mix", "fx", "REVERB MIX", ParameterValueType::Float, 0.0f, 100.0f, 30.0f, 1.0f, "%",
     ParameterScale::Linear, "Effects", "REVERB", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/patch_types.h",
     "Stereo reverb wet level"},
    {"fx.drive", "fx", "BUS DRIVE", ParameterValueType::Float, 0.0f, 100.0f, 10.0f, 1.0f, "%",
     ParameterScale::Linear, "Effects", "DRIVE", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/patch_types.h",
     "Post-synth bus soft saturation"},
    {"fx.master_tone", "fx", "TILT TONE", ParameterValueType::Float, -100.0f, 100.0f, 0.0f, 2.0f, "%",
     ParameterScale::Linear, "Effects", "EQ", nullptr, 0, kModTargetCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/patch_types.h",
     "Master 3-band / tilt EQ balance"},
    {"fx.routing", "fx", "FX CHAIN", ParameterValueType::Enum, 0.0f, 3.0f, 0.0f, 1.0f, "",
     ParameterScale::Linear, "Effects", "ROUTING", kFxRouting, 4, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "specs.md",
     "Master effect chain order and bypass mode"},

    // ── ARPEGGIATOR ──────────────────────────────────────────────────────
    {"arp.enable", "arp", "ARP ON", ParameterValueType::Bool, 0.0f, 1.0f, 0.0f, 1.0f, "",
     ParameterScale::Linear, "Arpeggiator", "MAIN", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/sequencer/include/arpeggiator.h",
     "Enable note arpeggiator"},
    {"arp.clock_source", "arp", "CLOCK SRC", ParameterValueType::Enum, 0.0f, 2.0f, 0.0f, 1.0f, "",
     ParameterScale::Linear, "Arpeggiator", "TIMING", kClockSources, 3, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/sequencer/include/clock_manager.h",
     "Arpeggiator clock sync source"},
    {"arp.rate", "arp", "ARP RATE", ParameterValueType::Enum, 0.0f, 5.0f, 3.0f, 1.0f, "",
     ParameterScale::Linear, "Arpeggiator", "TIMING", kArpRates, 6, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/sequencer/include/arpeggiator.h",
     "Arpeggiator rhythmic division"},
    {"arp.gate_pct", "arp", "GATE LEN", ParameterValueType::Int, 10.0f, 100.0f, 65.0f, 5.0f, "%",
     ParameterScale::Linear, "Arpeggiator", "TIMING", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/sequencer/include/arpeggiator.h",
     "Arpeggiated note gate duration percentage"},
    {"arp.octaves", "arp", "OCTAVES", ParameterValueType::Int, 1.0f, 4.0f, 2.0f, 1.0f, "oct",
     ParameterScale::Linear, "Arpeggiator", "PATTERN", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/sequencer/include/arpeggiator.h",
     "Arpeggiator octave span (1..4 octaves)"},
    {"arp.direction", "arp", "ARP DIR", ParameterValueType::Enum, 0.0f, 5.0f, 0.0f, 1.0f, "",
     ParameterScale::Linear, "Arpeggiator", "PATTERN", kArpDirs, 6, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/sequencer/include/arpeggiator.h",
     "Arpeggiator traversal direction"},
    {"arp.latch", "arp", "HOLD LATCH", ParameterValueType::Bool, 0.0f, 1.0f, 0.0f, 1.0f, "",
     ParameterScale::Linear, "Arpeggiator", "MAIN", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/sequencer/include/arpeggiator.h",
     "Keep arpeggiating after keys are released"},
    {"arp.swing_pct", "arp", "SWING", ParameterValueType::Int, 50.0f, 75.0f, 54.0f, 1.0f, "%",
     ParameterScale::Linear, "Arpeggiator", "TIMING", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/sequencer/include/arpeggiator.h",
     "Even/odd 16th-note swing percentage"},

    // ── SEQUENCER ────────────────────────────────────────────────────────
    {"seq.playing", "seq", "SEQ PLAY", ParameterValueType::Bool, 0.0f, 1.0f, 0.0f, 1.0f, "",
     ParameterScale::Linear, "Sequencer", "TRANSPORT", nullptr, 0, kCapControllable,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/sequencer/include/step_sequencer.h",
     "Step sequencer transport run state"},
    {"seq.pattern", "seq", "PATTERN ID", ParameterValueType::Int, 1.0f, 16.0f, 1.0f, 1.0f, "",
     ParameterScale::Linear, "Sequencer", "PATTERN", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/sequencer/include/step_sequencer.h",
     "Selected step sequencer pattern slot"},
    {"seq.length", "seq", "STEP LENGTH", ParameterValueType::Int, 1.0f, 16.0f, 16.0f, 1.0f, "st",
     ParameterScale::Linear, "Sequencer", "PATTERN", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/sequencer/include/step_sequencer.h",
     "Active pattern step length (1..16)"},
    {"seq.step_vel", "seq", "STEP VEL", ParameterValueType::Int, 1.0f, 127.0f, 100.0f, 1.0f, "",
     ParameterScale::Linear, "Sequencer", "STEP", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/sequencer/include/step_sequencer.h",
     "Selected step MIDI velocity"},
    {"seq.step_prob", "seq", "STEP PROB", ParameterValueType::Int, 0.0f, 100.0f, 100.0f, 5.0f, "%",
     ParameterScale::Linear, "Sequencer", "STEP", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/sequencer/include/step_sequencer.h",
     "Selected step trigger probability"},
    {"seq.step_ratchet", "seq", "RATCHET", ParameterValueType::Int, 1.0f, 4.0f, 1.0f, 1.0f, "x",
     ParameterScale::Linear, "Sequencer", "STEP", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/sequencer/include/step_sequencer.h",
     "Sub-step repeat count for selected step"},
    {"seq.transpose", "seq", "TRANSPOSE", ParameterValueType::Int, -24.0f, 24.0f, 0.0f, 1.0f, "st",
     ParameterScale::Linear, "Sequencer", "PATTERN", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/sequencer/include/step_sequencer.h",
     "Pattern global semitone transposition"},

    // ── DRUMS ────────────────────────────────────────────────────────────
    {"drums.track", "drums", "DRUM TRACK", ParameterValueType::Enum, 0.0f, 7.0f, 0.0f, 1.0f, "",
     ParameterScale::Linear, "Drums", "VOICE", kDrumVoices, 8, kCapControllable,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/pad_bank.h",
     "Selected percussion track (Pad 1..8)"},
    {"drums.level", "drums", "VOICE LEVEL", ParameterValueType::Int, 0.0f, 100.0f, 90.0f, 1.0f, "%",
     ParameterScale::Linear, "Drums", "VOICE", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/pad_bank.h",
     "Selected drum voice level"},
    {"drums.pan", "drums", "VOICE PAN", ParameterValueType::Int, -64.0f, 63.0f, 0.0f, 1.0f, "",
     ParameterScale::Linear, "Drums", "VOICE", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/pad_bank.h",
     "Selected drum voice stereo pan"},
    {"drums.tune_st", "drums", "VOICE TUNE", ParameterValueType::Int, -12.0f, 12.0f, 0.0f, 1.0f, "st",
     ParameterScale::Linear, "Drums", "VOICE", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/pad_bank.h",
     "Selected drum PCM/synth pitch tuning"},
    {"drums.decay_ms", "drums", "VOICE DECAY", ParameterValueType::Int, 20.0f, 2000.0f, 320.0f, 10.0f, "ms",
     ParameterScale::Logarithmic, "Drums", "VOICE", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/synth/include/pad_bank.h",
     "Selected drum decay envelope duration"},

    // ── HARMONY / SCALE ASSIST ───────────────────────────────────────────
    {"harmony.scale_enable", "harmony", "SCALE QUANT", ParameterValueType::Bool, 0.0f, 1.0f, 1.0f, 1.0f, "",
     ParameterScale::Linear, "Harmony", "SCALE", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/midi/include/scale_quantizer.h",
     "Enable real-time pitch quantization to active scale"},
    {"harmony.root_note", "harmony", "ROOT KEY", ParameterValueType::Enum, 0.0f, 11.0f, 0.0f, 1.0f, "",
     ParameterScale::Linear, "Harmony", "SCALE", kRootNotes, 12, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/midi/include/scale_quantizer.h",
     "Tonic root key (C..B) for Scale Quantizer and Chord Assist"},
    {"harmony.scale_type", "harmony", "SCALE TYPE", ParameterValueType::Enum, 0.0f, 10.0f, 2.0f, 1.0f, "",
     ParameterScale::Linear, "Harmony", "SCALE", kScaleTypes, 11, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/midi/include/scale_quantizer.h",
     "Active musical scale interval mask"},
    {"harmony.quant_mode", "harmony", "QUANT MODE", ParameterValueType::Enum, 0.0f, 5.0f, 0.0f, 1.0f, "",
     ParameterScale::Linear, "Harmony", "SCALE", kQuantizeModes, 6, kStdCtrl,
     FeatureStatus::Planned, "harmony.scale_enable", 1, "smk-s3/components/midi/include/scale_quantizer.h",
     "Rounding behavior for out-of-scale notes"},
    {"harmony.preserve_outside", "harmony", "ACCIDENTALS", ParameterValueType::Bool, 0.0f, 1.0f, 0.0f, 1.0f, "",
     ParameterScale::Linear, "Harmony", "SCALE", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, "harmony.scale_enable", 1, "specs.md",
     "Allow out-of-scale grace notes when velocity > threshold"},
    {"harmony.chord_enable", "harmony", "CHORD ON", ParameterValueType::Bool, 0.0f, 1.0f, 1.0f, 1.0f, "",
     ParameterScale::Linear, "Harmony", "CHORD", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/midi/include/chord_memory.h",
     "Trigger full chord voicing from single key press"},
    {"harmony.chord_type", "harmony", "CHORD TYPE", ParameterValueType::Enum, 0.0f, 13.0f, 2.0f, 1.0f, "",
     ParameterScale::Linear, "Harmony", "CHORD", kChordTypes, 14, kStdCtrl,
     FeatureStatus::Planned, "harmony.chord_enable", 1, "smk-s3/components/midi/include/chord_memory.h",
     "Chord interval structure (Major, Minor, Dim, Aug, Sus2/4, 7, Maj7, m7, Add9, 9)"},
    {"harmony.voicing", "harmony", "VOICING", ParameterValueType::Enum, 0.0f, 4.0f, 0.0f, 1.0f, "",
     ParameterScale::Linear, "Harmony", "CHORD", kChordVoicings, 5, kStdCtrl,
     FeatureStatus::Planned, "harmony.chord_enable", 1, "specs.md",
     "Chord voicing distribution across octaves"},
    {"harmony.inversion", "harmony", "INVERSION", ParameterValueType::Int, 0.0f, 3.0f, 0.0f, 1.0f, "inv",
     ParameterScale::Linear, "Harmony", "CHORD", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, "harmony.chord_enable", 1, "specs.md",
     "Chord inversion index (0=Root, 1=1st, 2=2nd, 3=3rd)"},
    {"harmony.spread", "harmony", "SPREAD", ParameterValueType::Int, 0.0f, 2.0f, 0.0f, 1.0f, "oct",
     ParameterScale::Linear, "Harmony", "CHORD", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, "harmony.chord_enable", 1, "specs.md",
     "Open voicing spread across upper octaves"},
    {"harmony.vel_behavior", "harmony", "CHORD VEL", ParameterValueType::Enum, 0.0f, 3.0f, 0.0f, 1.0f, "",
     ParameterScale::Linear, "Harmony", "CHORD", kVelBehaviors, 4, kStdCtrl,
     FeatureStatus::Planned, "harmony.chord_enable", 1, "specs.md",
     "Velocity scaling across generated chord notes"},
    {"harmony.dest_layer", "harmony", "DEST LAYER", ParameterValueType::Enum, 0.0f, 2.0f, 0.0f, 1.0f, "",
     ParameterScale::Linear, "Harmony", "CHORD", kDestLayers, 3, kStdCtrl,
     FeatureStatus::Planned, "harmony.chord_enable", 1, "specs.md",
     "Target synth layer for generated harmony notes"},

    // ── MIDI SETTINGS ────────────────────────────────────────────────────
    {"harmony.usb_channel", "harmony", "USB CHANNEL", ParameterValueType::Int, 1, 16, 1, 1, "ch",
     ParameterScale::Linear, "Harmony", "SOURCE", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "harmony.h", "Only this USB channel is transformed"},
    {"harmony.apply_touch", "harmony", "APPLY TOUCH", ParameterValueType::Bool, 0, 1, 1, 1, "",
     ParameterScale::Linear, "Harmony", "SOURCE", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "harmony.h", "Touch eligible when Harmony is enabled"},
    {"harmony.preset", "harmony", "QUICK SET", ParameterValueType::Enum, 0, 4, 0, 1, "",
     ParameterScale::Linear, "Harmony", "SOURCE", kHarmonyPresets, 5, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "harmony.h", "RAM only; applies a complete musical configuration"},

    {"midi.input_channel", "midi", "INPUT CH", ParameterValueType::Int, 0.0f, 16.0f, 0.0f, 1.0f, "ch",
     ParameterScale::Linear, "MIDI", "ROUTING", nullptr, 0, kStdCtrl,
     FeatureStatus::Available, nullptr, -1, "smk-p4/components/amy_runtime/usb_input.h",
     "Global MIDI receive channel (0 = OMNI 1..16)"},
    {"midi.vel_curve", "midi", "VEL CURVE", ParameterValueType::Enum, 0.0f, 3.0f, 0.0f, 1.0f, "",
     ParameterScale::Linear, "MIDI", "RESPONSE", kVelCurves, 4, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "specs.md",
     "Incoming keyboard velocity response curve"},
    {"midi.bend_range_st", "midi", "BEND RANGE", ParameterValueType::Int, 1.0f, 12.0f, 2.0f, 1.0f, "st",
     ParameterScale::Linear, "MIDI", "RESPONSE", nullptr, 0, kStdCtrl,
     FeatureStatus::Available, nullptr, -1, "smk-p4/components/amy_runtime/amy_runtime.cpp",
     "Pitch bend sensitivity (+/-2 st implemented in P4 standalone)"},
    {"midi.sustain_enable", "midi", "SUSTAIN CC64", ParameterValueType::Bool, 0.0f, 1.0f, 1.0f, 1.0f, "",
     ParameterScale::Linear, "MIDI", "FILTERS", nullptr, 0, kStdCtrl,
     FeatureStatus::Available, nullptr, -1, "smk-p4/components/amy_runtime/amy_runtime.cpp",
     "Enable sustain pedal CC64 handling"},
    {"midi.modwheel_enable", "midi", "MODWHL CC1", ParameterValueType::Bool, 0.0f, 1.0f, 1.0f, 1.0f, "",
     ParameterScale::Linear, "MIDI", "FILTERS", nullptr, 0, kStdCtrl,
     FeatureStatus::Available, nullptr, -1, "smk-p4/components/amy_runtime/amy_runtime.cpp",
     "Enable modulation wheel CC1 handling"},
    {"midi.cc_learn_active", "midi", "CC LEARN MODE", ParameterValueType::Bool, 0.0f, 1.0f, 0.0f, 1.0f, "",
     ParameterScale::Linear, "MIDI", "LEARN", nullptr, 0, kCapControllable,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/midi/include/midi_learn.h",
     "Interactive MIDI CC Learn capture state (implemented on S3; planned on P4)"},
    {"midi.clock_sync", "midi", "CLOCK SYNC", ParameterValueType::Bool, 0.0f, 1.0f, 0.0f, 1.0f, "",
     ParameterScale::Linear, "MIDI", "TIMING", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/sequencer/include/clock_manager.h",
     "Synchronize tempo to external 0xF8 MIDI timing clock"},

    // ── GLOBAL SETTINGS ──────────────────────────────────────────────────
    {"sys.brightness", "sys", "BRIGHTNESS", ParameterValueType::Int, 10.0f, 100.0f, 85.0f, 5.0f, "%",
     ParameterScale::Linear, "System", "DISPLAY", nullptr, 0, kCapControllable,
     FeatureStatus::Demo, nullptr, -1, "smk-p4/components/board/display_hal.h",
     "Display backlight level (on/off supported in board HAL; PWM dimmer planned)"},
    {"sys.theme", "sys", "UI THEME", ParameterValueType::Enum, 0.0f, 2.0f, 0.0f, 1.0f, "",
     ParameterScale::Linear, "System", "DISPLAY", kThemes, 3, kCapControllable,
     FeatureStatus::Demo, nullptr, -1, "smk-p4/components/ui_blueprint/include/ui_theme.h",
     "Visual theme preset"},
    {"sys.sample_rate_hz", "sys", "SAMPLE RATE", ParameterValueType::Int, 48000.0f, 48000.0f, 48000.0f, 0.0f, "Hz",
     ParameterScale::Linear, "System", "AUDIO", nullptr, 0, kCapReadOnly,
     FeatureStatus::Available, nullptr, -1, "smk-p4/components/amy_runtime/runtime_config.h",
     "Hardware I2S sample rate (fixed 48000 Hz, read-only)"},
    {"sys.master_tuning_hz", "sys", "A4 TUNING", ParameterValueType::Float, 430.0f, 450.0f, 440.0f, 0.5f, "Hz",
     ParameterScale::Linear, "System", "AUDIO", nullptr, 0, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "specs.md",
     "Global reference pitch for A4"},
    {"sys.boot_mode", "sys", "BOOT STATE", ParameterValueType::Enum, 0.0f, 2.0f, 1.0f, 1.0f, "",
     ParameterScale::Linear, "System", "STORAGE", kBootModes, 3, kStdCtrl,
     FeatureStatus::Planned, nullptr, -1, "smk-s3/components/storage/include/storage_manager.h",
     "Startup patch selection policy"}
};

constexpr size_t kDescriptorCount = sizeof(kDescriptors) / sizeof(kDescriptors[0]);

} // namespace

const ParameterDescriptor* ParameterRegistry::all(size_t& out_count) {
    out_count = kDescriptorCount;
    return kDescriptors;
}

size_t ParameterRegistry::count() {
    return kDescriptorCount;
}

const ParameterDescriptor* ParameterRegistry::findById(const char* id) {
    if (!id) return nullptr;
    for (size_t i = 0; i < kDescriptorCount; ++i) {
        if (std::strcmp(kDescriptors[i].id, id) == 0) {
            return &kDescriptors[i];
        }
    }
    return nullptr;
}

int ParameterRegistry::indexOf(const char* id) {
    if (!id) return -1;
    for (size_t i = 0; i < kDescriptorCount; ++i) {
        if (std::strcmp(kDescriptors[i].id, id) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool ParameterRegistry::hasDuplicateIds() {
    for (size_t i = 0; i < kDescriptorCount; ++i) {
        for (size_t j = i + 1; j < kDescriptorCount; ++j) {
            if (std::strcmp(kDescriptors[i].id, kDescriptors[j].id) == 0) {
                return true;
            }
        }
    }
    return false;
}

void ParameterRegistry::formatValue(const ParameterDescriptor& desc, const ParameterValue& val, char* buf, size_t cap) {
    if (!buf || cap == 0) return;
    buf[0] = '\0';
    switch (desc.type) {
        case ParameterValueType::Bool:
            std::snprintf(buf, cap, "%s", val.asBool() ? "ON" : "OFF");
            break;
        case ParameterValueType::Enum: {
            const int32_t idx = val.asInt();
            if (desc.enum_values && idx >= 0 && idx < static_cast<int32_t>(desc.enum_count)) {
                std::snprintf(buf, cap, "%s", desc.enum_values[idx]);
            } else {
                std::snprintf(buf, cap, "%ld", static_cast<long>(idx));
            }
            break;
        }
        case ParameterValueType::Int:
            if (desc.unit && desc.unit[0] != '\0') {
                std::snprintf(buf, cap, "%ld %s", static_cast<long>(val.asInt()), desc.unit);
            } else {
                std::snprintf(buf, cap, "%ld", static_cast<long>(val.asInt()));
            }
            break;
        case ParameterValueType::Float:
            if (desc.step >= 1.0f) {
                if (desc.unit && desc.unit[0] != '\0') {
                    std::snprintf(buf, cap, "%.0f %s", static_cast<double>(val.asFloat()), desc.unit);
                } else {
                    std::snprintf(buf, cap, "%.0f", static_cast<double>(val.asFloat()));
                }
            } else {
                if (desc.unit && desc.unit[0] != '\0') {
                    std::snprintf(buf, cap, "%.1f %s", static_cast<double>(val.asFloat()), desc.unit);
                } else {
                    std::snprintf(buf, cap, "%.1f", static_cast<double>(val.asFloat()));
                }
            }
            break;
        case ParameterValueType::String:
            std::snprintf(buf, cap, "%s", (val.type == ParameterValueType::String && val.s_val) ? val.s_val : "");
            break;
    }
}

} // namespace smk::p4::ui
