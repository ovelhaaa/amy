/**
 * AMY Studio - Interactive Additive Synthesis & 16-Harmonic Partials Editor
 * Provides real-time Fourier sum waveform visualization, harmonic drawbars,
 * phase control, spectral decay and AMY engine BYO_PARTIALS (w10) wire command generation.
 */

class AdditiveHarmonicEditor {
  constructor(containerId, canvasId) {
    this.container = document.getElementById(containerId);
    this.canvas = document.getElementById(canvasId);
    this.ctx = this.canvas ? this.canvas.getContext('2d') : null;

    this.numHarmonics = 16;
    // Harmonics: 1..16 (amplitude 0.0 .. 1.0, phase 0.0 .. 1.0)
    this.harmonics = Array.from({ length: this.numHarmonics }, (_, i) => ({
      num: i + 1,
      amp: i === 0 ? 1.0 : (1.0 / (i + 1)), // Default saw-like roll-off
      phase: 0.0,
      decayRate: 1.0 // envelope decay multiplier
    }));

    this.presets = {
      sine: { name: "Pure Sine", amps: [1.0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0] },
      saw: { name: "Sawtooth (1/n)", amps: Array.from({ length: 16 }, (_, i) => 1.0 / (i + 1)) },
      square: { name: "Square (Odd 1/n)", amps: Array.from({ length: 16 }, (_, i) => (i % 2 === 0 ? 1.0 / (i + 1) : 0)) },
      triangle: { name: "Triangle (1/n²)", amps: Array.from({ length: 16 }, (_, i) => (i % 2 === 0 ? 1.0 / Math.pow(i + 1, 2) : 0)) },
      organ888: { name: "Hammond 888000000", amps: [1.0, 0.9, 0.8, 0.7, 0.6, 0.5, 0.3, 0.2, 0.1, 0, 0, 0, 0, 0, 0, 0] },
      bell: { name: "Metallic Bell", amps: [1.0, 0.2, 0.8, 0.1, 0.6, 0.05, 0.5, 0.02, 0.4, 0.01, 0.3, 0.01, 0.2, 0.01, 0.1, 0.01] },
      formantAh: { name: "Vocal Formant 'Ah'", amps: [0.6, 1.0, 0.9, 0.4, 0.1, 0.05, 0.2, 0.4, 0.7, 0.5, 0.2, 0.1, 0.05, 0.02, 0.01, 0] }
    };

    if (this.container) this.renderUI();
    if (this.canvas) this.renderWaveform();
  }

  setHarmonicAmp(index, amp, dispatch = true) {
    if (index < 0 || index >= this.numHarmonics) return;
    this.harmonics[index].amp = Math.max(0, Math.min(1.0, amp));
    this.renderWaveform();
    if (dispatch) this.applyToEngine();
  }

  setHarmonicPhase(index, phase, dispatch = true) {
    if (index < 0 || index >= this.numHarmonics) return;
    this.harmonics[index].phase = Math.max(0, Math.min(1.0, phase));
    this.renderWaveform();
    if (dispatch) this.applyToEngine();
  }

  applyPreset(presetKey) {
    const p = this.presets[presetKey];
    if (!p) return;
    for (let i = 0; i < this.numHarmonics; i++) {
      this.harmonics[i].amp = p.amps[i] !== undefined ? p.amps[i] : 0;
      this.harmonics[i].phase = 0.0;
    }
    this.renderUI();
    this.renderWaveform();
    this.applyToEngine();
  }

  renderWaveform() {
    if (!this.canvas || !this.ctx) return;
    const ctx = this.ctx;
    const w = this.canvas.width;
    const h = this.canvas.height;

    ctx.fillStyle = '#0a0d14';
    ctx.fillRect(0, 0, w, h);

    // Grid
    ctx.strokeStyle = '#161c28';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(0, h / 2); ctx.lineTo(w, h / 2);
    ctx.stroke();

    // Compute Fourier Sum: s(t) = sum( a_n * sin(2*pi*n*t + phi_n) )
    const numSamples = w;
    const waveform = new Float32Array(numSamples);
    let maxVal = 0.001;

    for (let x = 0; x < numSamples; x++) {
      const t = x / numSamples;
      let sample = 0;
      for (let n = 0; n < this.numHarmonics; n++) {
        const hData = this.harmonics[n];
        if (hData.amp > 0.001) {
          const harmonicNum = n + 1;
          const phaseRad = hData.phase * 2 * Math.PI;
          sample += hData.amp * Math.sin(2 * Math.PI * harmonicNum * t + phaseRad);
        }
      }
      waveform[x] = sample;
      if (Math.abs(sample) > maxVal) maxVal = Math.abs(sample);
    }

    // Normalize and draw
    const normFactor = maxVal > 1.0 ? (1.0 / maxVal) : 1.0;
    const grad = ctx.createLinearGradient(0, 0, 0, h);
    grad.addColorStop(0, 'rgba(0, 240, 255, 0.4)');
    grad.addColorStop(1, 'rgba(0, 255, 136, 0.05)');

    ctx.fillStyle = grad;
    ctx.beginPath();
    ctx.moveTo(0, h / 2);

    for (let x = 0; x < numSamples; x++) {
      const normY = waveform[x] * normFactor;
      const y = (h / 2) - (normY * (h / 2 - 8));
      ctx.lineTo(x, y);
    }

    ctx.lineTo(w, h / 2);
    ctx.closePath();
    ctx.fill();

    // Stroke
    ctx.strokeStyle = '#00f0ff';
    ctx.lineWidth = 2;
    ctx.shadowBlur = 6;
    ctx.shadowColor = 'rgba(0, 240, 255, 0.6)';
    ctx.beginPath();
    for (let x = 0; x < numSamples; x++) {
      const normY = waveform[x] * normFactor;
      const y = (h / 2) - (normY * (h / 2 - 8));
      if (x === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.stroke();
    ctx.shadowBlur = 0;
  }

  renderUI() {
    if (!this.container) return;
    this.container.innerHTML = '';

    const grid = document.createElement('div');
    grid.className = 'additive-bars-grid';
    grid.style.display = 'grid';
    grid.style.gridTemplateColumns = 'repeat(16, 1fr)';
    grid.style.gap = '4px';
    grid.style.background = '#0d111a';
    grid.style.padding = '8px';
    grid.style.borderRadius = '6px';
    grid.style.border = '1px solid #1c2436';

    this.harmonics.forEach((h, idx) => {
      const col = document.createElement('div');
      col.style.display = 'flex';
      col.style.flexDirection = 'column';
      col.style.alignItems = 'center';
      col.style.gap = '4px';

      // Harmonic Label
      const lbl = document.createElement('span');
      lbl.innerText = `${h.num}x`;
      lbl.style.fontSize = '8px';
      lbl.style.color = idx === 0 ? 'var(--accent-green)' : '#94a3b8';
      lbl.style.fontFamily = 'monospace';

      // Vertical Slider Container
      const sliderBox = document.createElement('div');
      sliderBox.style.height = '110px';
      sliderBox.style.width = '14px';
      sliderBox.style.background = '#141a27';
      sliderBox.style.borderRadius = '3px';
      sliderBox.style.position = 'relative';
      sliderBox.style.overflow = 'hidden';
      sliderBox.style.cursor = 'ns-resize';
      sliderBox.style.border = '1px solid #232d42';

      // Fill Bar
      const fill = document.createElement('div');
      fill.style.position = 'absolute';
      fill.style.bottom = '0';
      fill.style.left = '0';
      fill.style.right = '0';
      fill.style.height = `${Math.round(h.amp * 100)}%`;
      fill.style.background = idx === 0 
        ? 'linear-gradient(to top, #00ff88, #00cc66)' 
        : 'linear-gradient(to top, #00f0ff, #0066ff)';
      fill.style.transition = 'height 0.05s ease';
      sliderBox.appendChild(fill);

      // Drag interaction
      const onMove = (e) => {
        const rect = sliderBox.getBoundingClientRect();
        const clientY = e.touches ? e.touches[0].clientY : e.clientY;
        const val = 1.0 - ((clientY - rect.top) / rect.height);
        this.setHarmonicAmp(idx, val);
        fill.style.height = `${Math.round(this.harmonics[idx].amp * 100)}%`;
        valText.innerText = Math.round(this.harmonics[idx].amp * 100);
      };

      sliderBox.addEventListener('mousedown', (e) => {
        e.preventDefault();
        onMove(e);
        const onMouseUp = () => {
          window.removeEventListener('mousemove', onMove);
          window.removeEventListener('mouseup', onMouseUp);
        };
        window.addEventListener('mousemove', onMove);
        window.addEventListener('mouseup', onMouseUp);
      });

      sliderBox.addEventListener('touchstart', (e) => {
        onMove(e);
        const onTouchEnd = () => {
          window.removeEventListener('touchmove', onMove);
          window.removeEventListener('touchend', onTouchEnd);
        };
        window.addEventListener('touchmove', onMove);
        window.addEventListener('touchend', onTouchEnd);
      }, { passive: true });

      // Amplitude Percentage readout
      const valText = document.createElement('span');
      valText.innerText = Math.round(h.amp * 100);
      valText.style.fontSize = '8px';
      valText.style.color = '#cbd5e1';
      valText.style.fontFamily = 'monospace';

      col.appendChild(lbl);
      col.appendChild(sliderBox);
      col.appendChild(valText);
      grid.appendChild(col);
    });

    this.container.appendChild(grid);
  }

  generateWireCommand() {
    // Generate AMY wire command for BYO_PARTIALS or additive multi-sine
    // Voice 0 is the base partials oscillator (w10)
    const activeCount = this.harmonics.filter(h => h.amp > 0.01).length;
    if (activeCount === 0) return "v0w0a1.0Z";

    let cmds = [];
    // Set base voice as BYO_PARTIALS (w10) with preset count = 16
    cmds.push(`v0w10p16a1.0Z`);

    // Configure individual partial oscillators (v1 to v16)
    for (let i = 0; i < this.numHarmonics; i++) {
      const h = this.harmonics[i];
      const oscId = i + 1;
      const ratio = (i + 1).toFixed(2);
      const amp = h.amp.toFixed(3);
      const phase = h.phase.toFixed(3);
      cmds.push(`v${oscId}w9I${ratio}a${amp}P${phase}Z`);
    }

    return cmds.join('');
  }

  applyToEngine() {
    const wire = this.generateWireCommand();
    if (window.amyAudioBridge) window.amyAudioBridge.sendWire(wire);
    if (window.esp32HardwareSync) window.esp32HardwareSync.sendWireIfConnected(wire);
  }

  exportData() {
    return this.harmonics.map(h => ({
      num: h.num,
      amp: h.amp,
      phase: h.phase
    }));
  }

  importData(data) {
    if (Array.isArray(data)) {
      data.forEach((d, i) => {
        if (i < this.numHarmonics) {
          this.harmonics[i].amp = d.amp !== undefined ? d.amp : 0;
          this.harmonics[i].phase = d.phase !== undefined ? d.phase : 0;
        }
      });
      this.renderUI();
      this.renderWaveform();
      this.applyToEngine();
    }
  }
}

window.AdditiveHarmonicEditor = AdditiveHarmonicEditor;
