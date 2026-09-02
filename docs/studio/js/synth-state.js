/**
 * AMY Studio - Central Synth State & Wire Command Generator
 * Synchronized with ESP32-S3 firmware (SMK-S3) AmyAdapter & PatchManager.
 * Targets polyphonic instrument i1 (Synth 1, MIDI ch 0) instead of raw oscillator v0.
 */

class SynthStateManager {
  constructor() {
    this.currentPatch = JSON.parse(JSON.stringify(DEFAULT_PATCH_TEMPLATE));
    this.undoStack = [];
    this.redoStack = [];
    this.listeners = [];
    this.isDirty = false;
    this.masterVolume = 1.0;
    this.masterLimiter = true;
    this.activeChannel = 0;
  }

  subscribe(listener) {
    this.listeners.push(listener);
  }

  notify(changedProp, source = 'user') {
    this.listeners.forEach(fn => fn(this.currentPatch, changedProp, source));
  }

  pushState() {
    this.undoStack.push(JSON.stringify(this.currentPatch));
    if (this.undoStack.length > 50) this.undoStack.shift();
    this.redoStack = [];
    this.isDirty = true;
  }

  undo() {
    if (this.undoStack.length === 0) return;
    this.redoStack.push(JSON.stringify(this.currentPatch));
    const previous = JSON.parse(this.undoStack.pop());
    this.currentPatch = previous;
    this.applyFullPatch();
    this.notify('all', 'undo');
  }

  redo() {
    if (this.redoStack.length === 0) return;
    this.undoStack.push(JSON.stringify(this.currentPatch));
    const next = JSON.parse(this.redoStack.pop());
    this.currentPatch = next;
    this.applyFullPatch();
    this.notify('all', 'redo');
  }

  setParam(key, value, pushHistory = true) {
    if (pushHistory) this.pushState();
    this.currentPatch[key] = value;
    this.applyParamChange(key, value);
    this.notify(key, 'user');
  }

  setMacro(macroIndex, value) {
    if (macroIndex < 0 || macroIndex >= 8) return;
    if (!this.currentPatch.macros) {
      this.currentPatch.macros = JSON.parse(JSON.stringify(DEFAULT_PATCH_TEMPLATE.macros));
    }
    this.currentPatch.macros[macroIndex].val = value;
    const norm = Math.max(0, Math.min(1, value / 100.0));

    // Evaluate macro mappings matching smk-s3 PatchManager::applyMacroToEngine
    switch (macroIndex) {
      case 0: { // CHAR: Filter Cutoff (100Hz - 12000Hz)
        const cutoff = 100.0 + norm * 11900.0;
        this.setParam('filter_cutoff', cutoff, false);
        break;
      }
      case 1: { // BRTE: Resonance (0.5 - 12.0)
        const res = 0.5 + norm * 11.5;
        this.setParam('filter_res', res, false);
        break;
      }
      case 2: { // MOTN: Chorus Level
        const chor = norm * 0.5;
        this.setParam('chorus_level', chor, false);
        break;
      }
      case 3: { // SHPE: Pulse Width / Duty
        const duty = 0.1 + norm * 0.8;
        this.setParam('duty', duty, false);
        break;
      }
      case 4: { // ATK: Amp Attack (2ms - 1000ms)
        const atk = 2.0 + norm * 998.0;
        this.setParam('amp_attack', atk, false);
        break;
      }
      case 5: { // REL: Amp Release (10ms - 3000ms)
        const rel = 10.0 + norm * 2990.0;
        this.setParam('amp_release', rel, false);
        break;
      }
      case 6: { // SPCE: Reverb Level (0.0 - 0.75)
        const rev = norm * 0.75;
        this.setParam('reverb_level', rev, false);
        break;
      }
      case 7: { // DRV: Safe Feedback / FM Drive (0.0 - 0.25)
        const fb = norm * 0.25;
        this.setParam('feedback', fb, false);
        break;
      }
    }

    this.notify(`macro_${macroIndex}`, 'user');
  }

  applyParamChange(key, val) {
    let wire = "";

    // Target polyphonic instrument i1 (Synth 1) so parameter changes apply to active voices
    switch (key) {
      case 'wave_type':
        wire = `i1w${val}Z`;
        break;
      case 'filter_type':
        wire = `i1G${val}Z`;
        break;
      case 'filter_cutoff':
        wire = `i1F${val.toFixed(2)}Z`;
        break;
      case 'filter_res':
        wire = `i1R${val.toFixed(2)}Z`;
        break;
      case 'duty':
        wire = `i1d${val.toFixed(3)}Z`;
        break;
      case 'feedback':
        wire = `i1b${val.toFixed(3)}Z`;
        break;
      case 'amp_attack':
      case 'amp_decay':
      case 'amp_sustain':
      case 'amp_release': {
        const a = Math.round(this.currentPatch.amp_attack);
        const d = Math.round(this.currentPatch.amp_decay);
        const s = this.currentPatch.amp_sustain.toFixed(3);
        const r = Math.round(this.currentPatch.amp_release);
        wire = `i1A${a},1,${d},${s},${r},0Z`;
        break;
      }
      case 'eg0_type':
        wire = `i1T${val}Z`;
        break;
      case 'eg1_type':
        wire = `i1X${val}Z`;
        break;
      case 'reverb_level':
      case 'reverb_size':
      case 'reverb_damp': {
        const rL = (this.currentPatch.reverb_level || 0).toFixed(2);
        const rS = (this.currentPatch.reverb_size || 0.6).toFixed(2);
        const rD = (this.currentPatch.reverb_damp || 0.4).toFixed(2);
        wire = `h${rL},${rS},${rD}Z`;
        break;
      }
      case 'chorus_level':
      case 'chorus_rate':
      case 'chorus_depth': {
        const cL = (this.currentPatch.chorus_level || 0).toFixed(2);
        const cR = (this.currentPatch.chorus_rate || 0.5).toFixed(2);
        const cD = (this.currentPatch.chorus_depth || 0.5).toFixed(2);
        wire = `k${cL},320,${cR},${cD}Z`;
        break;
      }
      case 'delay_level':
      case 'delay_time':
      case 'delay_feedback': {
        const dL = (this.currentPatch.delay_level || 0).toFixed(2);
        const dT = (this.currentPatch.delay_time || 350.0).toFixed(1);
        const dF = (this.currentPatch.delay_feedback || 0.3).toFixed(2);
        wire = `M${dL},${dT},743,${dF}Z`;
        break;
      }
      case 'eq_low':
      case 'eq_mid':
      case 'eq_high': {
        const eqL = (this.currentPatch.eq_low || 0).toFixed(1);
        const eqM = (this.currentPatch.eq_mid || 0).toFixed(1);
        const eqH = (this.currentPatch.eq_high || 0).toFixed(1);
        wire = `x${eqL},${eqM},${eqH}Z`;
        break;
      }

      // FM DX7 Operators: Ratio is 'I', Amplitude is 'a'
      case 'fm_op1_ratio': wire = `v7I${val.toFixed(3)}Z`; break;
      case 'fm_op1_amp':   wire = `v7a${val.toFixed(3)}Z`; break;
      case 'fm_op2_ratio': wire = `v6I${val.toFixed(3)}Z`; break;
      case 'fm_op2_amp':   wire = `v6a${val.toFixed(3)}Z`; break;
      case 'fm_op3_ratio': wire = `v5I${val.toFixed(3)}Z`; break;
      case 'fm_op3_amp':   wire = `v5a${val.toFixed(3)}Z`; break;
      case 'fm_op4_ratio': wire = `v4I${val.toFixed(3)}Z`; break;
      case 'fm_op4_amp':   wire = `v4a${val.toFixed(3)}Z`; break;
      case 'fm_op5_ratio': wire = `v3I${val.toFixed(3)}Z`; break;
      case 'fm_op5_amp':   wire = `v3a${val.toFixed(3)}Z`; break;
      case 'fm_op6_ratio': wire = `v2I${val.toFixed(3)}Z`; break;
      case 'fm_op6_amp':   wire = `v2a${val.toFixed(3)}Z`; break;
      case 'fm_op6_fb':    wire = `i1b${val.toFixed(3)}Z`; break;
    }

    if (wire.length > 0) {
      if (window.amyAudioBridge) window.amyAudioBridge.sendWire(wire);
      if (window.esp32HardwareSync) window.esp32HardwareSync.sendWireIfConnected(wire);
    }
  }

  applyFullPatch() {
    const p = this.currentPatch;
    const voices = p.voice_count || 8;
    console.log(`[Synth State] Applying complete patch: #${p.id} "${p.name}" (Synth 1, ${voices} voices)`);

    // 1. If it has a factory preset ID (Juno 0..127, DX7 128..255, PCM 256+)
    if (p.basePatch !== undefined || p.engine_patch !== undefined) {
      const basePatch = p.basePatch !== undefined ? p.basePatch : p.engine_patch;
      // Load onto Synth 1 with specified voice polyphony
      const wire = `i1iv${voices}K${basePatch}Z`;
      if (window.amyAudioBridge) window.amyAudioBridge.sendWire(wire);

      // Apply initial clean effects
      const rL = (p.reverb_level !== undefined ? p.reverb_level : 0.25).toFixed(2);
      const cL = (p.chorus_level !== undefined ? p.chorus_level : 0.0).toFixed(2);
      const dL = (p.delay_level !== undefined ? p.delay_level : 0.0).toFixed(2);
      const fxWire = `h${rL},0.6,0.4Zk${cL},320,0.5,0.5ZM${dL},350.0,743,0.3Z`;
      if (window.amyAudioBridge) window.amyAudioBridge.sendWire(fxWire);

      // If connected to ESP32-S3 DevKitC, sync active patch on hardware
      if (window.esp32HardwareSync && window.esp32HardwareSync.isConnected) {
        window.esp32HardwareSync.sendSerialCommand(`patch_select ${p.id}`);
      }
      return;
    }

    // 2. If it has a pre-baked wireCommand (e.g. DX7 SysEx Imports)
    if (p.wireCommand !== undefined) {
      if (window.amyAudioBridge) window.amyAudioBridge.sendWire(p.wireCommand);
      if (window.esp32HardwareSync) window.esp32HardwareSync.sendWireIfConnected(p.wireCommand);
      return;
    }

    // 3. Otherwise apply custom synthesis structure to Synth 1
    const a = Math.round(p.amp_attack || 10);
    const d = Math.round(p.amp_decay || 200);
    const s = (p.amp_sustain !== undefined ? p.amp_sustain : 0.7).toFixed(3);
    const r = Math.round(p.amp_release || 300);

    const rL = (p.reverb_level || 0).toFixed(2);
    const rS = (p.reverb_size || 0.6).toFixed(2);
    const rD = (p.reverb_damp || 0.4).toFixed(2);
    const cL = (p.chorus_level || 0).toFixed(2);
    const cR = (p.chorus_rate || 0.5).toFixed(2);
    const cD = (p.chorus_depth || 0.5).toFixed(2);
    const dL = (p.delay_level || 0).toFixed(2);
    const dT = (p.delay_time || 350.0).toFixed(1);
    const dF = (p.delay_feedback || 0.3).toFixed(2);
    const eqL = (p.eq_low || 0).toFixed(1);
    const eqM = (p.eq_mid || 0).toFixed(1);
    const eqH = (p.eq_high || 0).toFixed(1);

    const fullWire = `i1iv${voices}w${p.wave_type || 2}d${(p.duty || 0.5).toFixed(3)}b${(p.feedback || 0).toFixed(3)}G${p.filter_type || 1}F${(p.filter_cutoff || 2500).toFixed(2)}R${(p.filter_res || 1.2).toFixed(2)}A${a},1,${d},${s},${r},0T${p.eg0_type || 0}X${p.eg1_type || 0}Zh${rL},${rS},${rD}Zk${cL},320,${cR},${cD}ZM${dL},${dT},743,${dF}Zx${eqL},${eqM},${eqH}Z`;

    if (window.amyAudioBridge) window.amyAudioBridge.sendWire(fullWire);
    if (window.esp32HardwareSync) window.esp32HardwareSync.sendWireIfConnected(fullWire);
  }

  loadFactoryPatch(presetId) {
    const found = AMY_FACTORY_PATCHES.find(p => p.id === presetId);
    if (found) {
      this.pushState();

      const currentMacros = this.currentPatch.macros;
      const base = JSON.parse(JSON.stringify(DEFAULT_PATCH_TEMPLATE));
      Object.assign(base, found);

      if (!found.macros && currentMacros) {
        base.macros = JSON.parse(JSON.stringify(currentMacros));
      }

      this.currentPatch = base;
      this.applyFullPatch();
      this.notify('all', 'preset');
    }
  }
}

window.synthStateManager = new SynthStateManager();

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { SynthStateManager };
}
