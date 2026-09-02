/**
 * AMY Studio - Dedicated Roland Juno-106 Synthesizer Panel & DSP Translator
 * Implements the authentic Roland Juno-106 architecture using the formulas in amy/juno.py.
 * Targets polyphonic instrument i1 (Synth 1, MIDI ch 0) for ESP32 firmware parity.
 */

class Juno106PanelController {
  constructor() {
    this.state = {
      // DCO
      lfoDepth: 0,
      pwmVal: 50,
      pwmMode: 'MAN', // MAN or LFO
      pulseOn: true,
      sawOn: true,
      subLevel: 0,
      noiseLevel: 0,

      // HPF
      hpfFreq: 0, // 0=Off/Flat, 1=225Hz, 2=339Hz, 3=720Hz

      // VCF
      cutoff: 64, // 0..127
      resonance: 20, // 0..127
      envMod: 64, // 0..127
      envPolarity: 'POS', // POS or NEG
      vcfLfoMod: 0, // 0..127
      kbdTrack: 64, // 0..127

      // VCA
      vcaMode: 'ENV', // GATE or ENV

      // CHORUS
      chorusMode: 'I', // OFF, I, II, I+II

      // LFO
      lfoRate: 40,
      lfoDelay: 0
    };
  }

  setDcoParam(param, val) {
    this.state[param] = val;
    this.applyJunoState();
  }

  setVcfParam(param, val) {
    this.state[param] = val;
    this.applyJunoState();
  }

  setChorus(mode) {
    this.state.chorusMode = mode;
    this.applyJunoState();
  }

  setHpf(modeIndex) {
    this.state.hpfFreq = modeIndex;
    this.applyJunoState();
  }

  applyJunoState() {
    const s = this.state;
    
    // 1. Calculate Cutoff Hz using Juno log-curve
    const cutoffHz = 13.0 * Math.pow(2.0, (0.0938 * s.cutoff));
    
    // 2. Calculate Resonance Q (0.7 to 16.0 exponential)
    const resQ = 0.7 * Math.pow(2.0, (4.0 * (s.resonance / 127.0)));

    // 3. Calculate Pulse Duty
    const duty = Math.max(0.05, Math.min(0.95, s.pwmVal / 127.0));

    // 4. Calculate Chorus (I = 0.5Hz / 0.5 depth, II = 0.8Hz / 0.8 depth, I+II = 1.0Hz fast)
    let chorusLvl = 0.0, chorusRate = 0.5, chorusDepth = 0.5;
    if (s.chorusMode === 'I') {
      chorusLvl = 0.7; chorusRate = 0.5; chorusDepth = 0.4;
    } else if (s.chorusMode === 'II') {
      chorusLvl = 0.85; chorusRate = 0.8; chorusDepth = 0.7;
    } else if (s.chorusMode === 'I+II') {
      chorusLvl = 1.0; chorusRate = 1.2; chorusDepth = 0.9;
    }

    // 5. Select Waveform (SAW + PULSE mix)
    // In AMY, PULSE=1, SAW_DOWN=2
    let wave = 2; // Default to SAW_DOWN
    if (s.pulseOn && !s.sawOn) wave = 1; // PULSE
    if (!s.pulseOn && s.sawOn) wave = 2; // SAW
    if (s.pulseOn && s.sawOn) wave = 2;

    // Target polyphonic instrument i1 (Synth 1)
    const cmds = [
      `i1w${wave}d${duty.toFixed(3)}G1F${cutoffHz.toFixed(1)}R${resQ.toFixed(2)}Z`,
      `k${chorusLvl.toFixed(2)},320,${chorusRate.toFixed(2)},${chorusDepth.toFixed(2)}Z`
    ];

    const wire = cmds.join('');
    if (window.amyAudioBridge) window.amyAudioBridge.sendWire(wire);
    if (window.esp32HardwareSync) window.esp32HardwareSync.sendWireIfConnected(wire);
  }
}

window.juno106Panel = new Juno106PanelController();

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { Juno106PanelController };
}
