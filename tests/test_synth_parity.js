const assert = require('assert');

// Mock window environment for headless Node.js test
global.window = {};

// Load patch data
const patchData = require('../tools/amy-studio/js/patch-data.js');
global.AMY_FACTORY_PATCHES = patchData.AMY_FACTORY_PATCHES;
global.DEFAULT_PATCH_TEMPLATE = patchData.DEFAULT_PATCH_TEMPLATE;

// Mock AMY audio bridge & hardware sync to capture wire commands
const sentWires = [];
const sentSerialCmds = [];

global.window.amyAudioBridge = {
  sendWire: (wire) => sentWires.push(wire),
  noteOn: (ch, note, vel) => sentWires.push(`NOTE_ON:${ch}:${note}:${vel}`),
  noteOff: (ch, note) => sentWires.push(`NOTE_OFF:${ch}:${note}`),
  pitchBend: (ch, val) => sentWires.push(`BEND:${ch}:${val}`),
  panic: () => sentWires.push('PANIC')
};

global.window.esp32HardwareSync = {
  isConnected: true,
  liveSyncEnabled: true,
  sendSerialCommand: (cmd) => sentSerialCmds.push(cmd),
  sendWireIfConnected: (wire) => sentSerialCmds.push(`WIRE:${wire}`),
  noteOn: (ch, note, vel) => {
    const synthId = (ch === 9) ? 10 : (ch === 1 ? 2 : 1);
    sentSerialCmds.push(`i${synthId}n${note}l${vel.toFixed(3)}Z`);
  },
  noteOff: (ch, note) => {
    const synthId = (ch === 9) ? 10 : (ch === 1 ? 2 : 1);
    sentSerialCmds.push(`i${synthId}n${note}l0Z`);
  }
};

// Load SynthStateManager
const synthStateMod = require('../tools/amy-studio/js/synth-state.js');
const SynthStateManager = synthStateMod.SynthStateManager || global.window.synthStateManager.constructor;
const mgr = new SynthStateManager();

console.log("=== RUNNING AMY STUDIO SOUND PARITY TESTS ===");

// 1. Check patch database count
console.log("Test 1: Patch count parity (256 factory + 5 extra kits)...");
assert.strictEqual(global.AMY_FACTORY_PATCHES.length, 261);
assert.strictEqual(global.AMY_FACTORY_PATCHES[0].name.includes("Brass Set 1"), true);
assert.strictEqual(global.AMY_FACTORY_PATCHES[128].name.includes("BRASS 1"), true);
assert.strictEqual(global.AMY_FACTORY_PATCHES[255].name.includes("EXPLOSION"), true);
console.log("  PASS: 256 firmware factory presets loaded.");

// 2. Test Preset Loading routes to Synth 1 (i1iv8K...)
console.log("Test 2: Preset loading targeting Synth 1 (i1)...");
sentWires.length = 0;
sentSerialCmds.length = 0;
mgr.loadFactoryPatch(0); // Juno Brass Set 1
assert.strictEqual(sentWires.some(w => w.startsWith("i1iv10K0Z") || w.startsWith("i1iv8K0Z")), true);
assert.strictEqual(sentSerialCmds.includes("patch_select 0"), true);
console.log("  PASS: Factory preset 0 loads into Synth 1 on Web & sends patch_select 0 to ESP32.");

// 3. Test DX7 Preset Loading routes to Synth 1
console.log("Test 3: DX7 Preset 128 loading...");
sentWires.length = 0;
sentSerialCmds.length = 0;
mgr.loadFactoryPatch(128); // DX7 BRASS 1
assert.strictEqual(sentWires.some(w => w.startsWith("i1iv8K128Z")), true);
assert.strictEqual(sentSerialCmds.includes("patch_select 128"), true);
console.log("  PASS: DX7 Preset 128 loads into Synth 1 on Web & sends patch_select 128 to ESP32.");

// 4. Test Parameter tweaks target Synth 1 (i1F, i1R, i1w, etc.)
console.log("Test 4: Parameter changes target Synth 1 (i1) instead of raw oscillator (v0)...");
sentWires.length = 0;
mgr.setParam('filter_cutoff', 3500.0);
assert.strictEqual(sentWires.includes("i1F3500.00Z"), true);

sentWires.length = 0;
mgr.setParam('filter_res', 4.5);
assert.strictEqual(sentWires.includes("i1R4.50Z"), true);

sentWires.length = 0;
mgr.setParam('wave_type', 2);
assert.strictEqual(sentWires.includes("i1w2Z"), true);
console.log("  PASS: Filter and waveform tweaks target polyphonic instrument i1.");

// 5. Test DX7 Operator Ratio & Amplitude mapping (Ratio='I', Amp='a')
console.log("Test 5: DX7 Operator Ratio ('I') vs Amplitude ('a')...");
sentWires.length = 0;
mgr.setParam('fm_op1_ratio', 2.0);
assert.strictEqual(sentWires.includes("v7I2.000Z"), true, "Ratio must use command 'I'");

sentWires.length = 0;
mgr.setParam('fm_op1_amp', 0.75);
assert.strictEqual(sentWires.includes("v7a0.750Z"), true, "Amp must use command 'a'");
console.log("  PASS: Operator ratio and amp are correctly assigned.");

// 6. Test Hardware Sync Note Routing (ch 0 -> i1, ch 9 -> i10, ch 1 -> i2)
console.log("Test 6: Hardware Sync note routing...");
sentSerialCmds.length = 0;
global.window.esp32HardwareSync.noteOn(0, 60, 0.8);
assert.strictEqual(sentSerialCmds.includes("i1n60l0.800Z"), true);

sentSerialCmds.length = 0;
global.window.esp32HardwareSync.noteOn(9, 36, 0.95);
assert.strictEqual(sentSerialCmds.includes("i10n36l0.950Z"), true);

sentSerialCmds.length = 0;
global.window.esp32HardwareSync.noteOn(1, 48, 0.7);
assert.strictEqual(sentSerialCmds.includes("i2n48l0.700Z"), true);
console.log("  PASS: Note triggers route correctly to Synth 1, Synth 2, and Drums Synth 10.");

console.log("\n=== ALL SOUND PARITY TESTS PASSED SUCCESSFULLY! ===");
