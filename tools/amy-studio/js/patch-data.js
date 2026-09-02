/**
 * AMY Patch Database & Factory Presets
 */

const AMY_FACTORY_PATCHES = [
  // ── Juno-106 Presets (0..127) ──
  { id: 0,   name: "Juno Brass Set 1",       category: "Juno Brass",     type: "Juno", basePatch: 0 },
  { id: 1,   name: "Juno Brass Swell",       category: "Juno Brass",     type: "Juno", basePatch: 1 },
  { id: 2,   name: "Juno Trumpet",           category: "Juno Brass",     type: "Juno", basePatch: 2 },
  { id: 3,   name: "Juno Flutes",            category: "Juno Wind",      type: "Juno", basePatch: 3 },
  { id: 4,   name: "Juno Moving Strings",    category: "Juno Strings",   type: "Juno", basePatch: 4 },
  { id: 5,   name: "Juno Brass & Strings",   category: "Juno Strings",   type: "Juno", basePatch: 5 },
  { id: 6,   name: "Juno Choir",             category: "Juno Pad",       type: "Juno", basePatch: 6 },
  { id: 7,   name: "Juno Piano I",           category: "Juno Keys",      type: "Juno", basePatch: 7 },
  { id: 8,   name: "Juno Organ I",           category: "Juno Organ",     type: "Juno", basePatch: 8 },
  { id: 9,   name: "Juno Organ II",          category: "Juno Organ",     type: "Juno", basePatch: 9 },
  { id: 10,  name: "Juno Combo Organ",       category: "Juno Organ",     type: "Juno", basePatch: 10 },
  { id: 11,  name: "Juno Calliope",          category: "Juno Lead",      type: "Juno", basePatch: 11 },
  { id: 12,  name: "Juno Donald Pluck",      category: "Juno Pluck",     type: "Juno", basePatch: 12 },
  { id: 13,  name: "Juno Celeste",           category: "Juno Bell",      type: "Juno", basePatch: 13 },
  { id: 14,  name: "Juno Elect Piano I",     category: "Juno Keys",      type: "Juno", basePatch: 14 },
  { id: 15,  name: "Juno Elect Piano II",    category: "Juno Keys",      type: "Juno", basePatch: 15 },
  { id: 16,  name: "Juno Clock Chimes",      category: "Juno Bell",      type: "Juno", basePatch: 16 },
  { id: 17,  name: "Juno Steel Drums",       category: "Juno Perc",      type: "Juno", basePatch: 17 },
  { id: 18,  name: "Juno Xylophone",         category: "Juno Perc",      type: "Juno", basePatch: 18 },
  { id: 19,  name: "Juno Brass III",         category: "Juno Brass",     type: "Juno", basePatch: 19 },
  { id: 20,  name: "Juno Fanfare",           category: "Juno Brass",     type: "Juno", basePatch: 20 },
  { id: 21,  name: "Juno Strings III",       category: "Juno Strings",   type: "Juno", basePatch: 21 },
  { id: 22,  name: "Juno Pizzicato",         category: "Juno Pluck",     type: "Juno", basePatch: 22 },
  { id: 23,  name: "Juno High Strings",      category: "Juno Strings",   type: "Juno", basePatch: 23 },
  { id: 31,  name: "Juno Synth Bass I",      category: "Juno Bass",      type: "Juno", basePatch: 31 },
  { id: 32,  name: "Juno Lead I",            category: "Juno Lead",      type: "Juno", basePatch: 32 },
  { id: 33,  name: "Juno Lead II",           category: "Juno Lead",      type: "Juno", basePatch: 33 },
  { id: 34,  name: "Juno Lead III",          category: "Juno Lead",      type: "Juno", basePatch: 34 },
  { id: 35,  name: "Juno Funky II",          category: "Juno Lead",      type: "Juno", basePatch: 35 },
  { id: 36,  name: "Juno Synth Bass II",     category: "Juno Bass",      type: "Juno", basePatch: 36 },
  { id: 47,  name: "Juno Synth Pad",         category: "Juno Pad",       type: "Juno", basePatch: 47 },
  { id: 64,  name: "Juno Strings (Bank B)",  category: "Juno Strings",   type: "Juno", basePatch: 64 },

  // ── DX7 FM Presets (128..255) ──
  { id: 128, name: "DX7 E. Piano 1",         category: "DX7 Keys",       type: "DX7",  basePatch: 128 },
  { id: 129, name: "DX7 E. Piano 2",         category: "DX7 Keys",       type: "DX7",  basePatch: 129 },
  { id: 130, name: "DX7 Marimba",            category: "DX7 Perc",       type: "DX7",  basePatch: 130 },
  { id: 131, name: "DX7 Tubular Bell",       category: "DX7 Bell",       type: "DX7",  basePatch: 131 },
  { id: 132, name: "DX7 Clavinet",           category: "DX7 Keys",       type: "DX7",  basePatch: 132 },
  { id: 133, name: "DX7 Harpsichord",        category: "DX7 Keys",       type: "DX7",  basePatch: 133 },
  { id: 134, name: "DX7 Acoustic Piano",     category: "DX7 Keys",       type: "DX7",  basePatch: 134 },
  { id: 135, name: "DX7 Caliope",            category: "DX7 Lead",       type: "DX7",  basePatch: 135 },
  { id: 136, name: "DX7 Synth Brass 1",      category: "DX7 Brass",      type: "DX7",  basePatch: 136 },
  { id: 137, name: "DX7 Synth Brass 2",      category: "DX7 Brass",      type: "DX7",  basePatch: 137 },
  { id: 138, name: "DX7 Synth Brass 3",      category: "DX7 Brass",      type: "DX7",  basePatch: 138 },
  { id: 139, name: "DX7 Strings 1",          category: "DX7 Strings",    type: "DX7",  basePatch: 139 },
  { id: 140, name: "DX7 Strings 2",          category: "DX7 Strings",    type: "DX7",  basePatch: 140 },
  { id: 141, name: "DX7 Flute 1",            category: "DX7 Wind",       type: "DX7",  basePatch: 141 },
  { id: 142, name: "DX7 Bass 1 (Lately)",    category: "DX7 Bass",       type: "DX7",  basePatch: 142 },
  { id: 143, name: "DX7 Bass 2",             category: "DX7 Bass",       type: "DX7",  basePatch: 143 },
  { id: 144, name: "DX7 Harmonica 1",        category: "DX7 Lead",       type: "DX7",  basePatch: 144 },
  { id: 145, name: "DX7 Jazz Guitar",        category: "DX7 Pluck",      type: "DX7",  basePatch: 145 },

  // ── AMY Additive / PCM / Drums ──
  { id: 256, name: "AMY Additive Piano",     category: "Acoustic",       type: "Additive", basePatch: 256 },
  { id: 384, name: "Standard Drum Kit",      category: "Drums",          type: "PCM/GM",   basePatch: 384 },
  { id: 385, name: "TR-808 Drum Kit",        category: "Drums",          type: "PCM/GM",   basePatch: 385 },
  { id: 386, name: "TR-909 Drum Kit",        category: "Drums",          type: "PCM/GM",   basePatch: 386 },
  { id: 387, name: "LinnDrum Kit",           category: "Drums",          type: "PCM/GM",   basePatch: 387 }
];

const DEFAULT_PATCH_TEMPLATE = {
  id: 0,
  name: "Init Patch",
  category: "Lead",
  author: "AMY Studio",
  voice_count: 8,
  engine_patch: 0,
  transpose: 0,
  
  // Oscillator 0 (Main)
  wave_type: 1, // SAW_DOWN
  base_freq: 440.0,
  duty: 0.5,
  feedback: 0.0,
  phase: 0.0,
  portamento_ms: 0,
  
  // Filter
  filter_type: 1, // LPF
  filter_cutoff: 2500.0,
  filter_res: 1.2,
  filter_env_amt: 0.5,
  filter_keytrack: 0.3,
  
  // Envelope 0 (Amp)
  amp_attack: 10.0,
  amp_decay: 200.0,
  amp_sustain: 0.7,
  amp_release: 300.0,
  eg0_type: 0, // Normal
  
  // Envelope 1 (Filter/Mod)
  eg1_attack: 20.0,
  eg1_decay: 350.0,
  eg1_sustain: 0.2,
  eg1_release: 400.0,
  eg1_type: 0,
  
  // Effects (Bus 0)
  eq_low: 0.0,
  eq_mid: 0.0,
  eq_high: 0.0,
  chorus_level: 0.0,
  chorus_rate: 0.5,
  chorus_depth: 0.5,
  delay_level: 0.0,
  delay_time: 400.0,
  delay_feedback: 0.3,
  reverb_level: 0.2,
  reverb_size: 0.8,
  reverb_damp: 0.5,

  // 8 SMK-S3 Macros
  macros: [
    { name: "CHAR", val: 50.0, label: "Character / Cutoff" },
    { name: "BRTE", val: 50.0, label: "Brightness / Wave" },
    { name: "MOTN", val: 30.0, label: "Motion / LFO Depth" },
    { name: "SHAP", val: 50.0, label: "Shape / Envelope" },
    { name: "ATK",  val: 10.0, label: "Attack Time" },
    { name: "REL",  val: 40.0, label: "Release Time" },
    { name: "SPCE", val: 25.0, label: "Space / Reverb" },
    { name: "DRV",  val: 0.0,  label: "Drive / Saturation" }
  ],

  // 16-Harmonic Partials (Additive)
  harmonics: [
    { num: 1, amp: 1.0, phase: 0.0 },
    { num: 2, amp: 0.5, phase: 0.0 },
    { num: 3, amp: 0.33, phase: 0.0 },
    { num: 4, amp: 0.25, phase: 0.0 },
    { num: 5, amp: 0.2, phase: 0.0 },
    { num: 6, amp: 0.16, phase: 0.0 },
    { num: 7, amp: 0.14, phase: 0.0 },
    { num: 8, amp: 0.12, phase: 0.0 },
    { num: 9, amp: 0.11, phase: 0.0 },
    { num: 10, amp: 0.1, phase: 0.0 },
    { num: 11, amp: 0.09, phase: 0.0 },
    { num: 12, amp: 0.08, phase: 0.0 },
    { num: 13, amp: 0.07, phase: 0.0 },
    { num: 14, amp: 0.07, phase: 0.0 },
    { num: 15, amp: 0.06, phase: 0.0 },
    { num: 16, amp: 0.06, phase: 0.0 }
  ],

  // 6-Slot Modulation Matrix
  mod_matrix: [
    { source: 'lfo1', dest: 'cutoff', amount: 0.35 },
    { source: 'eg1',  dest: 'cutoff', amount: 0.60 },
    { source: 'vel',  dest: 'amp',    amount: 0.75 },
    { source: 'modwheel', dest: 'pitch', amount: 0.15 },
    { source: 'none', dest: 'none',   amount: 0.0 },
    { source: 'none', dest: 'none',   amount: 0.0 }
  ],

  // Keyboard Layer & Split Configuration
  layer_config: {
    mode: 'single', // 'single', 'layer', 'split'
    split_point: 60,
    balance: 0.5,
    layer_b_patch: 4,
    layer_b_detune: 12,
    layer_b_transpose: 0
  }
};
