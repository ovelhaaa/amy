/**
 * AMY Studio - M-VAVE SMK25 V2 Interactive 8-Pad Performance Controller
 * Replicates the hardware PadManager (components/synth/src/pad_bank.cpp):
 * Bank A: 808 Drums (MIDI Ch 10 / AMY Synth 10)
 * Bank B: Melodic C4..C5 Pentatonic (MIDI Ch 1 / AMY Synth 1)
 * Bank C: Polyphonic Chords (MIDI Ch 1 / AMY Synth 1)
 * Bank D: Performance FX (Stutters, Sweeps, Pitch Drop, Space Wash, Mute)
 */

class PadBankManager {
  constructor(containerId) {
    this.container = document.getElementById(containerId);
    this.activeBank = 0; // 0=A, 1=B, 2=C, 3=D

    this.banks = [
      {
        id: 'A',
        name: 'BANK A: DRUMS',
        pads: [
          { name: 'KICK',      notes: [36], ch: 9, color: '#ff0055' },
          { name: 'RIM',       notes: [37], ch: 9, color: '#ff5500' },
          { name: 'SNARE',     notes: [38], ch: 9, color: '#ffaa00' },
          { name: 'CLAP',      notes: [39], ch: 9, color: '#ffea00' },
          { name: 'E-SNARE',   notes: [40], ch: 9, color: '#00ff88' },
          { name: 'LOW TOM',   notes: [41], ch: 9, color: '#00f0ff' },
          { name: 'CLOSED HH', notes: [42], ch: 9, color: '#0077ff' },
          { name: 'HIGH TOM',  notes: [43], ch: 9, color: '#aa00ff' }
        ]
      },
      {
        id: 'B',
        name: 'BANK B: MELODIC',
        pads: [
          { name: 'C4', notes: [60], ch: 0, color: '#00f0ff' },
          { name: 'D4', notes: [62], ch: 0, color: '#00f0ff' },
          { name: 'E4', notes: [64], ch: 0, color: '#00f0ff' },
          { name: 'G4', notes: [67], ch: 0, color: '#00f0ff' },
          { name: 'A4', notes: [69], ch: 0, color: '#00f0ff' },
          { name: 'C5', notes: [72], ch: 0, color: '#00f0ff' },
          { name: 'D5', notes: [74], ch: 0, color: '#00f0ff' },
          { name: 'E5', notes: [76], ch: 0, color: '#00f0ff' }
        ]
      },
      {
        id: 'C',
        name: 'BANK C: CHORDS',
        pads: [
          { name: 'C MAJ',  notes: [60, 64, 67],     ch: 0, color: '#ffb700' },
          { name: 'A MIN',  notes: [57, 60, 64],     ch: 0, color: '#ffb700' },
          { name: 'F MAJ7', notes: [53, 57, 60, 64], ch: 0, color: '#ffb700' },
          { name: 'G 7TH',  notes: [55, 59, 62, 65], ch: 0, color: '#ffb700' },
          { name: 'D MIN',  notes: [62, 65, 69],     ch: 0, color: '#ffb700' },
          { name: 'E MIN7', notes: [52, 55, 59, 62], ch: 0, color: '#ffb700' },
          { name: 'C SUS4', notes: [60, 65, 67],     ch: 0, color: '#ffb700' },
          { name: 'B DIM',  notes: [59, 62, 65],     ch: 0, color: '#ffb700' }
        ]
      },
      {
        id: 'D',
        name: 'BANK D: PERF FX',
        pads: [
          { name: 'STUTTER 16', isFx: true, fxType: 'stutter16', color: '#ff0077' },
          { name: 'STUTTER 32', isFx: true, fxType: 'stutter32', color: '#ff0077' },
          { name: 'LPF SWEEP',  isFx: true, fxType: 'lpf',       color: '#ffaa00' },
          { name: 'HPF SWEEP',  isFx: true, fxType: 'hpf',       color: '#00ff88' },
          { name: 'PITCH DROP', isFx: true, fxType: 'pitch',     color: '#aa00ff' },
          { name: 'REVERB WASH',isFx: true, fxType: 'reverb',    color: '#00f0ff' },
          { name: 'DELAY ROLL', isFx: true, fxType: 'delay',     color: '#0077ff' },
          { name: 'MUTE CUT',   isFx: true, fxType: 'mute',      color: '#ff3333' }
        ]
      }
    ];

    this.fxInterval = null;
    this.originalState = null;

    if (this.container) this.render();
  }

  setBank(bankIdx) {
    if (bankIdx < 0 || bankIdx >= 4) return;
    this.activeBank = bankIdx;
    const bLetters = ['a', 'b', 'c', 'd'];
    if (window.esp32HardwareSync && window.esp32HardwareSync.isConnected) {
      window.esp32HardwareSync.sendSerialCommand(`pad_bank ${bLetters[bankIdx]}`);
    }
    this.render();
  }

  handlePadDown(padIdx) {
    const pad = this.banks[this.activeBank].pads[padIdx];
    if (!pad) return;

    const el = document.getElementById(`perf_pad_${padIdx}`);
    if (el) el.classList.add('active');

    if (pad.isFx) {
      this.triggerFxDown(pad.fxType);
      return;
    }

    // Trigger notes
    for (const n of pad.notes) {
      if (window.amyAudioBridge) window.amyAudioBridge.noteOn(pad.ch, n, 0.9);
      if (window.esp32HardwareSync) window.esp32HardwareSync.noteOn(pad.ch, n, 0.9);
    }
  }

  handlePadUp(padIdx) {
    const pad = this.banks[this.activeBank].pads[padIdx];
    if (!pad) return;

    const el = document.getElementById(`perf_pad_${padIdx}`);
    if (el) el.classList.remove('active');

    if (pad.isFx) {
      this.triggerFxUp(pad.fxType);
      return;
    }

    // Release notes
    for (const n of pad.notes) {
      if (window.amyAudioBridge) window.amyAudioBridge.noteOff(pad.ch, n);
      if (window.esp32HardwareSync) window.esp32HardwareSync.noteOff(pad.ch, n);
    }
  }

  triggerFxDown(fxType) {
    const p = window.synthStateManager ? window.synthStateManager.currentPatch : null;
    switch (fxType) {
      case 'lpf':
        if (window.amyAudioBridge) window.amyAudioBridge.sendWire(`i1F300Z`);
        if (window.esp32HardwareSync) window.esp32HardwareSync.sendWireIfConnected(`i1F300Z`);
        break;
      case 'hpf':
        if (window.amyAudioBridge) window.amyAudioBridge.sendWire(`i1G3F1200Z`);
        if (window.esp32HardwareSync) window.esp32HardwareSync.sendWireIfConnected(`i1G3F1200Z`);
        break;
      case 'pitch':
        if (window.amyAudioBridge) window.amyAudioBridge.pitchBend(0, -0.8);
        if (window.esp32HardwareSync) window.esp32HardwareSync.pitchBend(0, -0.8);
        break;
      case 'reverb':
        if (window.amyAudioBridge) window.amyAudioBridge.sendWire(`h0.95,0.95,0.8Z`);
        if (window.esp32HardwareSync) window.esp32HardwareSync.sendWireIfConnected(`h0.95,0.95,0.8Z`);
        break;
      case 'delay':
        if (window.amyAudioBridge) window.amyAudioBridge.sendWire(`M0.7,180.0,743,0.7Z`);
        if (window.esp32HardwareSync) window.esp32HardwareSync.sendWireIfConnected(`M0.7,180.0,743,0.7Z`);
        break;
      case 'mute':
        if (window.amyAudioBridge) window.amyAudioBridge.panic();
        if (window.esp32HardwareSync) window.esp32HardwareSync.sendSerialCommand('panic');
        break;
    }
  }

  triggerFxUp(fxType) {
    const p = window.synthStateManager ? window.synthStateManager.currentPatch : null;
    const baseCutoff = p ? p.filter_cutoff : 2500;
    const baseFiltType = p ? p.filter_type : 1;

    switch (fxType) {
      case 'lpf':
      case 'hpf':
        if (window.amyAudioBridge) window.amyAudioBridge.sendWire(`i1G${baseFiltType}F${baseCutoff.toFixed(2)}Z`);
        if (window.esp32HardwareSync) window.esp32HardwareSync.sendWireIfConnected(`i1G${baseFiltType}F${baseCutoff.toFixed(2)}Z`);
        break;
      case 'pitch':
        if (window.amyAudioBridge) window.amyAudioBridge.pitchBend(0, 0.0);
        if (window.esp32HardwareSync) window.esp32HardwareSync.pitchBend(0, 0.0);
        break;
      case 'reverb':
        const rL = (p ? p.reverb_level : 0.25).toFixed(2);
        if (window.amyAudioBridge) window.amyAudioBridge.sendWire(`h${rL},0.6,0.4Z`);
        if (window.esp32HardwareSync) window.esp32HardwareSync.sendWireIfConnected(`h${rL},0.6,0.4Z`);
        break;
      case 'delay':
        const dL = (p ? p.delay_level : 0.0).toFixed(2);
        if (window.amyAudioBridge) window.amyAudioBridge.sendWire(`M${dL},350.0,743,0.3Z`);
        if (window.esp32HardwareSync) window.esp32HardwareSync.sendWireIfConnected(`M${dL},350.0,743,0.3Z`);
        break;
    }
  }

  render() {
    if (!this.container) return;
    this.container.innerHTML = '';

    const wrap = document.createElement('div');
    wrap.className = 'pad-bank-wrapper';
    wrap.style.display = 'flex';
    wrap.style.flexDirection = 'column';
    wrap.style.gap = '6px';
    wrap.style.background = '#0e121b';
    wrap.style.padding = '8px 12px';
    wrap.style.borderRadius = '8px';
    wrap.style.border = '1px solid #1e2638';

    // Bank Navigation Tabs
    const bankBar = document.createElement('div');
    bankBar.style.display = 'flex';
    bankBar.style.justifyContent = 'space-between';
    bankBar.style.alignItems = 'center';

    const title = document.createElement('div');
    title.innerText = 'SMK25 V2 PADS';
    title.style.fontSize = '10px';
    title.style.fontWeight = '700';
    title.style.letterSpacing = '1px';
    title.style.color = '#94a3b8';

    const btnGroup = document.createElement('div');
    btnGroup.style.display = 'flex';
    btnGroup.style.gap = '4px';

    ['A: DRUMS', 'B: MELODIC', 'C: CHORDS', 'D: FX'].forEach((bLabel, idx) => {
      const bBtn = document.createElement('button');
      bBtn.className = `btn btn-sm ${this.activeBank === idx ? 'btn-primary' : 'btn-secondary'}`;
      bBtn.style.fontSize = '9px';
      bBtn.style.padding = '2px 8px';
      bBtn.innerText = bLabel;
      bBtn.addEventListener('click', () => this.setBank(idx));
      btnGroup.appendChild(bBtn);
    });

    bankBar.appendChild(title);
    bankBar.appendChild(btnGroup);
    wrap.appendChild(bankBar);

    // 8 RGB Pads Grid
    const padsGrid = document.createElement('div');
    padsGrid.style.display = 'grid';
    padsGrid.style.gridTemplateColumns = 'repeat(8, 1fr)';
    padsGrid.style.gap = '6px';

    const currentBank = this.banks[this.activeBank];
    currentBank.pads.forEach((pad, idx) => {
      const pBtn = document.createElement('div');
      pBtn.id = `perf_pad_${idx}`;
      pBtn.className = 'performance-pad';
      pBtn.style.height = '48px';
      pBtn.style.background = '#141a27';
      pBtn.style.border = `1px solid #232d42`;
      pBtn.style.borderBottom = `3px solid ${pad.color}`;
      pBtn.style.borderRadius = '4px';
      pBtn.style.display = 'flex';
      pBtn.style.flexDirection = 'column';
      pBtn.style.justifyContent = 'center';
      pBtn.style.alignItems = 'center';
      pBtn.style.cursor = 'pointer';
      pBtn.style.userSelect = 'none';
      pBtn.style.transition = 'all 0.08s ease';

      pBtn.innerHTML = `
        <span style="font-size: 9px; font-weight: 700; color: #fff; pointer-events: none;">${pad.name}</span>
        <span style="font-size: 7px; color: #64748b; font-family: monospace; pointer-events: none;">PAD ${idx + 1}</span>
      `;

      const onDown = (e) => {
        e.preventDefault();
        pBtn.style.background = pad.color;
        pBtn.style.boxShadow = `0 0 12px ${pad.color}`;
        this.handlePadDown(idx);
      };

      const onUp = (e) => {
        e.preventDefault();
        pBtn.style.background = '#141a27';
        pBtn.style.boxShadow = 'none';
        this.handlePadUp(idx);
      };

      pBtn.addEventListener('mousedown', onDown);
      pBtn.addEventListener('mouseup', onUp);
      pBtn.addEventListener('mouseleave', onUp);
      pBtn.addEventListener('touchstart', onDown, { passive: false });
      pBtn.addEventListener('touchend', onUp, { passive: false });

      padsGrid.appendChild(pBtn);
    });

    wrap.appendChild(padsGrid);
    this.container.appendChild(wrap);
  }
}

window.PadBankManager = PadBankManager;

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { PadBankManager };
}
