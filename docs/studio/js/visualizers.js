/**
 * AMY Studio - Realtime Audio Oscilloscope, FFT Spectrum & ESP32 OLED Display Renderer
 */

class AmyVisualizerEngine {
  constructor() {
    this.oscCanvas = null;
    this.oscCtx = null;
    this.specCanvas = null;
    this.specCtx = null;
    this.oledCanvas = null;
    this.oledCtx = null;
    this.isRendering = false;
    this.currentOledScreen = "Home"; // Home, System, MidiMonitor, Sequencer, Pads
  }

  init(oscCanvasId, specCanvasId, oledCanvasId) {
    this.oscCanvas = document.getElementById(oscCanvasId);
    if (this.oscCanvas) this.oscCtx = this.oscCanvas.getContext('2d');

    this.specCanvas = document.getElementById(specCanvasId);
    if (this.specCanvas) this.specCtx = this.specCanvas.getContext('2d');

    this.oledCanvas = document.getElementById(oledCanvasId);
    if (this.oledCanvas) {
      this.oledCtx = this.oledCanvas.getContext('2d');
      this.oledCtx.imageSmoothingEnabled = false;
    }

    this.startRenderLoop();
  }

  startRenderLoop() {
    if (this.isRendering) return;
    this.isRendering = true;

    const render = () => {
      this.drawOscilloscope();
      this.drawSpectrum();
      this.drawOledDisplay();
      requestAnimationFrame(render);
    };
    requestAnimationFrame(render);
  }

  drawOscilloscope() {
    if (!this.oscCtx || !this.oscCanvas) return;
    const ctx = this.oscCtx;
    const w = this.oscCanvas.width;
    const h = this.oscCanvas.height;

    ctx.fillStyle = '#060a10';
    ctx.fillRect(0, 0, w, h);

    // Grid Lines
    ctx.strokeStyle = '#141c2b';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(0, h / 2); ctx.lineTo(w, h / 2);
    ctx.moveTo(w / 2, 0); ctx.lineTo(w / 2, h);
    ctx.stroke();

    // Waveform
    const data = window.amyAudioBridge ? window.amyAudioBridge.getWaveformData() : null;
    if (!data) return;

    ctx.strokeStyle = '#00f0ff';
    ctx.lineWidth = 2;
    ctx.shadowBlur = 8;
    ctx.shadowColor = 'rgba(0, 240, 255, 0.6)';
    ctx.beginPath();

    const sliceW = w / data.length;
    let x = 0;
    for (let i = 0; i < data.length; i++) {
      const v = data[i] / 128.0;
      const y = (v * h) / 2;
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
      x += sliceW;
    }
    ctx.stroke();
    ctx.shadowBlur = 0;
  }

  drawSpectrum() {
    if (!this.specCtx || !this.specCanvas) return;
    const ctx = this.specCtx;
    const w = this.specCanvas.width;
    const h = this.specCanvas.height;

    ctx.fillStyle = '#060a10';
    ctx.fillRect(0, 0, w, h);

    const data = window.amyAudioBridge ? window.amyAudioBridge.getSpectrumData() : null;
    if (!data) return;

    const bars = 48;
    const barW = (w / bars) - 1;

    for (let i = 0; i < bars; i++) {
      const val = data[i * 2] / 255.0;
      const barH = val * h;
      const x = i * (barW + 1);
      const y = h - barH;

      const grad = ctx.createLinearGradient(0, h, 0, 0);
      grad.addColorStop(0, '#00ff88');
      grad.addColorStop(0.6, '#ffb700');
      grad.addColorStop(1.0, '#ff3366');

      ctx.fillStyle = grad;
      ctx.fillRect(x, y, barW, barH);
    }
  }

  drawOledDisplay() {
    if (!this.oledCtx || !this.oledCanvas) return;
    const ctx = this.oledCtx;
    const canvas = this.oledCanvas;

    ctx.fillStyle = '#000000';
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    ctx.save();
    ctx.scale(2, 2); // Exact 284 x 76 virtual resolution

    switch (this.currentOledScreen) {
      case 'System':
        this.renderOledSystem(ctx);
        break;
      case 'MidiMonitor':
        this.renderOledMidi(ctx);
        break;
      case 'Sequencer':
        this.renderOledSequencer(ctx);
        break;
      default:
        this.renderOledHome(ctx);
        break;
    }

    ctx.restore();
  }

  renderOledHome(ctx) {
    const p = window.synthStateManager ? window.synthStateManager.currentPatch : { id: 0, name: "Init" };
    ctx.fillStyle = '#FFFFFF';
    ctx.font = '10px "JetBrains Mono", monospace';
    const pStr = `#${String(p.id).padStart(3, '0')} ${p.name.padEnd(12, ' ')} [POLY] 120BPM`;
    ctx.fillText(pStr, 2, 10);

    // Status Dots
    ctx.fillStyle = '#00FF88';
    ctx.fillRect(245, 3, 5, 5);
    ctx.fillStyle = '#FFFFFF';
    ctx.fillText('USB', 252, 10);

    // Knob Gauges for 8 Macros
    ctx.fillStyle = '#00F3FF';
    ctx.fillText(`KNOB BANK A (MACROS)`, 2, 21);

    ctx.strokeStyle = '#333333';
    ctx.lineWidth = 1;
    ctx.beginPath(); ctx.moveTo(0, 23.5); ctx.lineTo(284, 23.5); ctx.stroke();

    const xs = [2, 37, 72, 107, 142, 177, 212, 247];
    const labels = ["CHAR", "BRTE", "MOTN", "SHAP", "ATK", "REL", "SPCE", "DRV"];

    for (let i = 0; i < 8; i++) {
      const x = xs[i];
      const val = p.macros ? p.macros[i].val : 50;
      const norm = val / 100.0;

      ctx.fillStyle = '#777777';
      ctx.font = '8px "JetBrains Mono", monospace';
      ctx.fillText(labels[i], x, 32);

      ctx.strokeStyle = '#555555';
      ctx.strokeRect(x, 35, 32, 38);

      const fillH = Math.round(norm * 36);
      ctx.fillStyle = '#00FFAA';
      ctx.fillRect(x + 1, 35 + (36 - fillH), 30, fillH);
    }
  }

  renderOledSystem(ctx) {
    ctx.fillStyle = '#00F3FF';
    ctx.font = '10px "JetBrains Mono", monospace';
    ctx.fillText('SYSTEM DIAGNOSTICS [ESP32-S3 DevKitC]', 2, 10);

    ctx.strokeStyle = '#444444';
    ctx.beginPath(); ctx.moveTo(0, 12.5); ctx.lineTo(284, 12.5); ctx.stroke();

    ctx.fillStyle = '#FFAA00';
    ctx.fillText('CPU: 240MHz (Core 1 Audio / Core 0 Ctrl)', 2, 25);
    ctx.fillStyle = '#00FF88';
    ctx.fillText('DSP LOAD: 12.4% | VOICES: 08/16 | UNDERRUNS: 0', 2, 40);
    ctx.fillStyle = '#FFFFFF';
    ctx.fillText('INT RAM: 320 KB | PSRAM: 8.0 MB | FLASH: 16 MB', 2, 55);
    ctx.fillStyle = '#AAAAAA';
    ctx.fillText('I2S DAC: PCM5102A 48kHz Stereo | USB: SMK25 V2', 2, 70);
  }

  renderOledMidi(ctx) {
    ctx.fillStyle = '#00F3FF';
    ctx.font = '10px "JetBrains Mono", monospace';
    ctx.fillText('MIDI MONITOR (USB HOST & VIRTUAL)', 2, 10);

    ctx.strokeStyle = '#444444';
    ctx.beginPath(); ctx.moveTo(0, 12.5); ctx.lineTo(284, 12.5); ctx.stroke();

    ctx.fillStyle = '#00FF88';
    ctx.fillText('CH01 | NOTE ON  | NOTE: 060 (C4) | VEL: 100', 2, 26);
    ctx.fillStyle = '#FFFFFF';
    ctx.fillText('CH01 | CC 074   | CUTOFF FREQ    | VAL: 085', 2, 40);
    ctx.fillStyle = '#888888';
    ctx.fillText('CH01 | PITCH BND| BEND: 0.000    | VAL: 8192', 2, 54);
    ctx.fillStyle = '#666666';
    ctx.fillText('CH01 | NOTE OFF | NOTE: 060 (C4) | VEL: 000', 2, 68);
  }

  renderOledSequencer(ctx) {
    const seq = window.amyStudioSequencer;
    ctx.fillStyle = '#00F3FF';
    ctx.font = '10px "JetBrains Mono", monospace';
    ctx.fillText(`SEQUENCER: 120.0 BPM [${seq && seq.isPlaying ? 'RUNNING' : 'STOPPED'}]`, 2, 10);

    ctx.strokeStyle = '#444444';
    ctx.beginPath(); ctx.moveTo(0, 12.5); ctx.lineTo(284, 12.5); ctx.stroke();

    for (let s = 0; s < 16; s++) {
      const x = 2 + s * 17 + Math.floor(s / 4) * 4;
      const isCur = seq && seq.currentStep === s;

      ctx.fillStyle = '#888888';
      ctx.font = '8px "JetBrains Mono", monospace';
      ctx.fillText(String(s + 1).padStart(2, '0'), x, 28);

      ctx.strokeStyle = isCur ? '#FFAA00' : '#444444';
      ctx.strokeRect(x, 32, 12, 16);

      if (seq && seq.drumTracks[0].pattern[s]) {
        ctx.fillStyle = '#00FF88';
        ctx.fillRect(x + 2, 34, 8, 12);
      }
    }
  }
}

window.amyVisualizerEngine = new AmyVisualizerEngine();
