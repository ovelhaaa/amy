/**
 * AMY Studio - Central Synth State & Wire Command Generator
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
    this.currentPatch.macros[macroIndex].val = value;
    
    // Evaluate macro mappings
    switch (macroIndex) {
      case 0: // CHAR: Cutoff
        const cutoff = 100 + (value / 100.0) * 9900;
        this.setParam('filter_cutoff', cutoff, false);
        break;
      case 1: // BRTE: Resonance / Waveform
        const res = 0.5 + (value / 100.0) * 12.0;
        this.setParam('filter_res', res, false);
        break;
      case 4: // ATK: Amp Attack
        const atk = (value / 100.0) * 1000.0;
        this.setParam('amp_attack', atk, false);
        break;
      case 5: // REL: Amp Release
        const rel = (value / 100.0) * 3000.0;
        this.setParam('amp_release', rel, false);
        break;
      case 6: // SPCE: Reverb Level
        const rev = (value / 100.0) * 0.9;
        this.setParam('reverb_level', rev, false);
        break;
      case 7: // DRV: EQ Low/Mid Boost
        const eqL = (value / 100.0) * 12.0;
        this.setParam('eq_low', eqL, false);
        break;
    }
    
    this.notify(`macro_${macroIndex}`, 'user');
  }

  applyParamChange(key, val) {
    let wire = "";

    switch (key) {
      case 'wave_type':
        wire = `v0w${val}Z`;
        break;
      case 'filter_type':
        wire = `v0G${val}Z`;
        break;
      case 'filter_cutoff':
        // Wire F sets filter cutoff frequency
        wire = `v0F${val.toFixed(2)}Z`;
        break;
      case 'filter_res':
        wire = `v0R${val.toFixed(2)}Z`;
        break;
      case 'duty':
        wire = `v0d${val.toFixed(3)}Z`;
        break;
      case 'feedback':
        wire = `v0b${val.toFixed(3)}Z`;
        break;
      case 'amp_attack':
      case 'amp_decay':
      case 'amp_sustain':
      case 'amp_release':
        // Breakpoints for EG0: A<time1>,<val1>,<time2>,<val2>...
        const a = Math.round(this.currentPatch.amp_attack);
        const d = Math.round(this.currentPatch.amp_decay);
        const s = this.currentPatch.amp_sustain.toFixed(3);
        const r = Math.round(this.currentPatch.amp_release);
        wire = `v0A${a},1,${d},${s},${r},0Z`;
        break;
      case 'eg0_type':
        wire = `v0T${val}Z`;
        break;
      case 'eg1_type':
        wire = `v0X${val}Z`;
        break;
      case 'reverb_level':
      case 'reverb_size':
      case 'reverb_damp':
        const rL = this.currentPatch.reverb_level.toFixed(2);
        const rS = this.currentPatch.reverb_size.toFixed(2);
        const rD = this.currentPatch.reverb_damp.toFixed(2);
        wire = `y0h${rL},${rS},${rD}Z`;
        break;
      case 'chorus_level':
      case 'chorus_rate':
      case 'chorus_depth':
        const cL = this.currentPatch.chorus_level.toFixed(2);
        const cR = this.currentPatch.chorus_rate.toFixed(2);
        const cD = this.currentPatch.chorus_depth.toFixed(2);
        wire = `y0k${cL},320,${cR},${cD}Z`;
        break;
      case 'delay_level':
      case 'delay_time':
      case 'delay_feedback':
        const dL = this.currentPatch.delay_level.toFixed(2);
        const dT = this.currentPatch.delay_time.toFixed(1);
        const dF = this.currentPatch.delay_feedback.toFixed(2);
        wire = `y0M${dL},${dT},743,${dF}Z`;
        break;
      case 'eq_low':
      case 'eq_mid':
      case 'eq_high':
        const eqL = this.currentPatch.eq_low.toFixed(1);
        const eqM = this.currentPatch.eq_mid.toFixed(1);
        const eqH = this.currentPatch.eq_high.toFixed(1);
        wire = `y0x${eqL},${eqM},${eqH}Z`;
        break;
      
      // FM DX7 Operators (Mapped to voices 2-7)
      case 'fm_op1_ratio': wire = `v7a${val.toFixed(3)}Z`; break;
      case 'fm_op1_amp':   wire = `v7I${val.toFixed(3)}Z`; break;
      case 'fm_op2_ratio': wire = `v6a${val.toFixed(3)}Z`; break;
      case 'fm_op2_amp':   wire = `v6I${val.toFixed(3)}Z`; break;
      case 'fm_op3_ratio': wire = `v5a${val.toFixed(3)}Z`; break;
      case 'fm_op3_amp':   wire = `v5I${val.toFixed(3)}Z`; break;
      case 'fm_op4_ratio': wire = `v4a${val.toFixed(3)}Z`; break;
      case 'fm_op4_amp':   wire = `v4I${val.toFixed(3)}Z`; break;
      case 'fm_op5_ratio': wire = `v3a${val.toFixed(3)}Z`; break;
      case 'fm_op5_amp':   wire = `v3I${val.toFixed(3)}Z`; break;
      case 'fm_op6_ratio': wire = `v2a${val.toFixed(3)}Z`; break;
      case 'fm_op6_amp':   wire = `v2I${val.toFixed(3)}Z`; break;
      case 'fm_op6_fb':    wire = `v0b${val.toFixed(3)}Z`; break;
    }

    if (wire.length > 0) {
      if (window.amyAudioBridge) window.amyAudioBridge.sendWire(wire);
      if (window.esp32HardwareSync) window.esp32HardwareSync.sendWireIfConnected(wire);
    }
  }

  applyFullPatch() {
    console.log(`[Synth State] Applying complete patch: #${this.currentPatch.id} "${this.currentPatch.name}"`);
    
    // If it's a built-in factory patch (Juno/DX7/Piano), we can load it via Program Change / K command
    if (this.currentPatch.basePatch !== undefined) {
      const wire = `i0K${this.currentPatch.basePatch}Z`;
      if (window.amyAudioBridge) window.amyAudioBridge.sendWire(wire);
      if (window.esp32HardwareSync) window.esp32HardwareSync.sendWireIfConnected(wire);
      return;
    }

    // If it has a custom pre-baked wireCommand (e.g. DX7 SysEx Imports)
    if (this.currentPatch.wireCommand !== undefined) {
      if (window.amyAudioBridge) window.amyAudioBridge.sendWire(this.currentPatch.wireCommand);
      if (window.esp32HardwareSync) window.esp32HardwareSync.sendWireIfConnected(this.currentPatch.wireCommand);
      return;
    }

    // Otherwise apply full custom synthesis structure
    const p = this.currentPatch;
    const a = Math.round(p.amp_attack);
    const d = Math.round(p.amp_decay);
    const s = p.amp_sustain.toFixed(3);
    const r = Math.round(p.amp_release);

    let cmds = [
      `v0w${p.wave_type}f440d${p.duty.toFixed(3)}b${p.feedback.toFixed(3)}G${p.filter_type}F${p.filter_cutoff.toFixed(2)}R${p.filter_res.toFixed(2)}`,
      `A${a},1,${d},${s},${r},0`,
      `T${p.eg0_type}X${p.eg1_type}`,
      `y0h${p.reverb_level.toFixed(2)},${p.reverb_size.toFixed(2)},${p.reverb_damp.toFixed(2)}`,
      `y0k${p.chorus_level.toFixed(2)},320,${p.chorus_rate.toFixed(2)},${p.chorus_depth.toFixed(2)}`,
      `y0M${p.delay_level.toFixed(2)},${p.delay_time.toFixed(1)},743,${p.delay_feedback.toFixed(2)}`,
      `y0x${p.eq_low.toFixed(1)},${p.eq_mid.toFixed(1)},${p.eq_high.toFixed(1)}Z`
    ];

    const fullWire = cmds.join('');
    if (window.amyAudioBridge) window.amyAudioBridge.sendWire(fullWire);
    if (window.esp32HardwareSync) window.esp32HardwareSync.sendWireIfConnected(fullWire);
  }

  loadFactoryPatch(presetId) {
    const found = AMY_FACTORY_PATCHES.find(p => p.id === presetId);
    if (found) {
      this.pushState();
      
      // Preserve macros if not provided by preset
      const currentMacros = this.currentPatch.macros;
      
      // Create a fresh template and merge the found patch
      const base = JSON.parse(JSON.stringify(DEFAULT_PATCH_TEMPLATE));
      Object.assign(base, found);
      
      if (!found.macros && currentMacros) {
        base.macros = JSON.parse(JSON.stringify(currentMacros));
      }
      
      this.currentPatch = base;
      this.applyFullPatch();
      this.notify('all', 'preset_loaded');
    }
  }
}

window.synthStateManager = new SynthStateManager();
