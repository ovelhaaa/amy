/**
 * AMY Studio - Keyboard Split, Dual Layer & Multi-Timbral Routing Manager
 * Enables Single, Dual Layer (Unison/Stacked) and Split Keyboard modes with custom split points,
 * independent patch selection, volume balance and cents detuning.
 */

class LayerSplitManager {
  constructor(containerId) {
    this.container = document.getElementById(containerId);
    this.mode = 'single'; // 'single', 'layer', 'split'
    this.splitPoint = 60; // Middle C (C3 / C4 depending on octave standard)
    this.balance = 0.5; // 0.0 = 100% Layer A, 1.0 = 100% Layer B

    this.layerA = {
      name: "Layer A (Lower / Main)",
      patchId: 0,
      volume: 1.0,
      transpose: 0, // semitones
      channel: 0
    };

    this.layerB = {
      name: "Layer B (Upper / Sub)",
      patchId: 4,
      volume: 0.85,
      detuneCents: 12, // cents detuning for thick unison
      transpose: 0,
      channel: 1
    };

    if (this.container) this.renderUI();
  }

  setMode(mode) {
    this.mode = mode;
    this.renderUI();
    this.updateKeyboardVisuals();
    console.log(`[Layer/Split Manager] Mode set to: ${mode.toUpperCase()}`);
  }

  setSplitPoint(note) {
    this.splitPoint = Math.max(36, Math.min(84, note));
    this.updateKeyboardVisuals();
    const lbl = document.getElementById('splitPointLabel');
    if (lbl) lbl.innerText = this.getNoteName(this.splitPoint);
  }

  getNoteName(midiNote) {
    const names = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
    const oct = Math.floor(midiNote / 12) - 1;
    return `${names[midiNote % 12]}${oct} (#${midiNote})`;
  }

  routeNoteOn(channel, note, velocity) {
    if (!window.amyAudioBridge) return;

    if (this.mode === 'single') {
      window.amyAudioBridge.noteOn(channel, note, velocity);
      if (window.esp32HardwareSync) window.esp32HardwareSync.noteOn(channel, note, velocity);
    } else if (this.mode === 'layer') {
      // Trigger Layer A (Channel 0)
      const noteA = Math.max(0, Math.min(127, note + this.layerA.transpose));
      const velA = velocity * this.layerA.volume * (1.0 - (this.balance - 0.5));
      window.amyAudioBridge.noteOn(0, noteA, velA);
      if (window.esp32HardwareSync) window.esp32HardwareSync.noteOn(0, noteA, velA);

      // Trigger Layer B (Channel 1 with detune)
      const noteB = Math.max(0, Math.min(127, note + this.layerB.transpose));
      const velB = velocity * this.layerB.volume * (this.balance + 0.5);
      window.amyAudioBridge.noteOn(1, noteB, velB);
      if (window.esp32HardwareSync) window.esp32HardwareSync.noteOn(1, noteB, velB);
    } else if (this.mode === 'split') {
      if (note < this.splitPoint) {
        // Lower Zone -> Layer A (e.g. Bass)
        const noteA = Math.max(0, Math.min(127, note + this.layerA.transpose));
        window.amyAudioBridge.noteOn(0, noteA, velocity * this.layerA.volume);
        if (window.esp32HardwareSync) window.esp32HardwareSync.noteOn(0, noteA, velocity * this.layerA.volume);
      } else {
        // Upper Zone -> Layer B (e.g. Lead / Pad)
        const noteB = Math.max(0, Math.min(127, note + this.layerB.transpose));
        window.amyAudioBridge.noteOn(1, noteB, velocity * this.layerB.volume);
        if (window.esp32HardwareSync) window.esp32HardwareSync.noteOn(1, noteB, velocity * this.layerB.volume);
      }
    }
  }

  routeNoteOff(channel, note) {
    if (!window.amyAudioBridge) return;

    if (this.mode === 'single') {
      window.amyAudioBridge.noteOff(channel, note);
      if (window.esp32HardwareSync) window.esp32HardwareSync.noteOff(channel, note);
    } else if (this.mode === 'layer') {
      window.amyAudioBridge.noteOff(0, Math.max(0, Math.min(127, note + this.layerA.transpose)));
      window.amyAudioBridge.noteOff(1, Math.max(0, Math.min(127, note + this.layerB.transpose)));
      if (window.esp32HardwareSync) {
        window.esp32HardwareSync.noteOff(0, note);
        window.esp32HardwareSync.noteOff(1, note);
      }
    } else if (this.mode === 'split') {
      if (note < this.splitPoint) {
        window.amyAudioBridge.noteOff(0, Math.max(0, Math.min(127, note + this.layerA.transpose)));
        if (window.esp32HardwareSync) window.esp32HardwareSync.noteOff(0, note);
      } else {
        window.amyAudioBridge.noteOff(1, Math.max(0, Math.min(127, note + this.layerB.transpose)));
        if (window.esp32HardwareSync) window.esp32HardwareSync.noteOff(1, note);
      }
    }
  }

  updateKeyboardVisuals() {
    // Color-code the virtual keyboard keys based on split zones
    document.querySelectorAll('.piano-key').forEach(key => {
      const note = parseInt(key.dataset.note);
      if (isNaN(note)) return;

      key.classList.remove('zone-lower', 'zone-upper');
      if (this.mode === 'split') {
        if (note < this.splitPoint) {
          key.classList.add('zone-lower');
        } else {
          key.classList.add('zone-upper');
        }
      }
    });
  }

  renderUI() {
    if (!this.container) return;
    this.container.innerHTML = '';

    const card = document.createElement('div');
    card.className = 'layer-split-card';
    card.style.display = 'flex';
    card.style.flexDirection = 'column';
    card.style.gap = '10px';

    // Header & Mode Selector Buttons
    const header = document.createElement('div');
    header.style.display = 'flex';
    header.style.justifyContent = 'space-between';
    header.style.alignItems = 'center';
    header.innerHTML = `<span class="card-title">MODO DE TECLADO & <span class="highlight">CAMADAS</span></span>`;

    const modeBtns = document.createElement('div');
    modeBtns.style.display = 'flex';
    modeBtns.style.gap = '4px';

    const modes = [
      { id: 'single', label: 'SINGLE' },
      { id: 'layer',  label: 'DUAL LAYER' },
      { id: 'split',  label: 'SPLIT KEYBOARD' }
    ];

    modes.forEach(m => {
      const btn = document.createElement('button');
      btn.className = `btn ${this.mode === m.id ? 'btn-primary' : 'btn-secondary'}`;
      btn.style.fontSize = '9px';
      btn.style.padding = '4px 8px';
      btn.innerText = m.label;
      btn.addEventListener('click', () => this.setMode(m.id));
      modeBtns.appendChild(btn);
    });

    header.appendChild(modeBtns);
    card.appendChild(header);

    // Detail Panel based on Mode
    if (this.mode === 'split') {
      const splitRow = document.createElement('div');
      splitRow.style.display = 'flex';
      splitRow.style.alignItems = 'center';
      splitRow.style.justifyContent = 'space-between';
      splitRow.style.background = '#0a0d14';
      splitRow.style.padding = '8px';
      splitRow.style.borderRadius = '4px';
      splitRow.style.border = '1px solid #1c2436';

      splitRow.innerHTML = `
        <span style="font-size: 10px; color: #94a3b8;">PONTO DE DIVISÃO (SPLIT POINT):</span>
        <span id="splitPointLabel" style="font-size: 10px; font-weight: 700; color: var(--accent-gold); font-family: monospace;">
          ${this.getNoteName(this.splitPoint)}
        </span>
      `;

      const slider = document.createElement('input');
      slider.type = 'range';
      slider.min = '36';
      slider.max = '84';
      slider.value = this.splitPoint;
      slider.className = 'juno-slider';
      slider.style.width = '120px';
      slider.addEventListener('input', (e) => this.setSplitPoint(parseInt(e.target.value)));
      splitRow.appendChild(slider);

      card.appendChild(splitRow);
    } else if (this.mode === 'layer') {
      const layerRow = document.createElement('div');
      layerRow.style.display = 'flex';
      layerRow.style.alignItems = 'center';
      layerRow.style.justifyContent = 'space-between';
      layerRow.style.background = '#0a0d14';
      layerRow.style.padding = '8px';
      layerRow.style.borderRadius = '4px';
      layerRow.style.border = '1px solid #1c2436';

      layerRow.innerHTML = `
        <span style="font-size: 10px; color: #94a3b8;">BALANÇO A ⟷ B:</span>
        <span style="font-size: 9px; color: #00f0ff;">LAYER A (50%)</span>
      `;

      const balSlider = document.createElement('input');
      balSlider.type = 'range';
      balSlider.min = '0';
      balSlider.max = '100';
      balSlider.value = Math.round(this.balance * 100);
      balSlider.className = 'juno-slider';
      balSlider.style.width = '100px';
      balSlider.addEventListener('input', (e) => {
        this.balance = parseFloat(e.target.value) / 100.0;
      });
      layerRow.appendChild(balSlider);

      card.appendChild(layerRow);
    }

    this.container.appendChild(card);
  }
}

window.LayerSplitManager = LayerSplitManager;
