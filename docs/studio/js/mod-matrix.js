/**
 * AMY Studio - 6-Slot Modulation Matrix Engine
 * Maps Sources (LFO1, LFO2, EG0, EG1, Velocity, ModWheel, PitchBend)
 * to Destinations (Pitch, Filter Cutoff, Resonance, PWM, Amp/Tremolo, Pan, FM)
 * and translates routes directly into AMY polynomial coefficient vectors (f, F, a, d).
 */

class ModulationMatrixManager {
  constructor(containerId) {
    this.container = document.getElementById(containerId);
    this.numSlots = 6;

    this.sources = [
      { id: 'none',      label: '-- OFF --' },
      { id: 'lfo1',      label: 'LFO 1 (Tri/Sine)' },
      { id: 'lfo2',      label: 'LFO 2 (Pitch Vib)' },
      { id: 'eg0',       label: 'EG 0 (Amp ADSR)' },
      { id: 'eg1',       label: 'EG 1 (Filter ADSR)' },
      { id: 'vel',       label: 'Key Velocity' },
      { id: 'modwheel',  label: 'Mod Wheel (CC1)' },
      { id: 'pitchbend', label: 'Pitch Bend' }
    ];

    this.destinations = [
      { id: 'none',      label: '-- OFF --' },
      { id: 'pitch',     label: 'Pitch / Freq (Semis)' },
      { id: 'cutoff',    label: 'Filter Cutoff (Hz)' },
      { id: 'resonance', label: 'Filter Resonance (Q)' },
      { id: 'pwm',       label: 'Pulse Width (PWM)' },
      { id: 'amp',       label: 'Amp Level (Tremolo)' },
      { id: 'pan',       label: 'Stereo Pan Spread' },
      { id: 'fm_mod',    label: 'FM Mod Index / Ratio' }
    ];

    this.slots = [
      { source: 'lfo1', dest: 'cutoff', amount: 0.35 },
      { source: 'eg1',  dest: 'cutoff', amount: 0.60 },
      { source: 'vel',  dest: 'amp',    amount: 0.75 },
      { source: 'modwheel', dest: 'pitch', amount: 0.15 },
      { source: 'none', dest: 'none',   amount: 0.0 },
      { source: 'none', dest: 'none',   amount: 0.0 }
    ];

    if (this.container) this.renderUI();
  }

  setSlot(index, source, dest, amount, dispatch = true) {
    if (index < 0 || index >= this.numSlots) return;
    if (source !== undefined) this.slots[index].source = source;
    if (dest !== undefined) this.slots[index].dest = dest;
    if (amount !== undefined) this.slots[index].amount = Math.max(-1.0, Math.min(1.0, amount));

    if (dispatch) this.applyToEngine();
  }

  computeAmyWireCommands() {
    // Default base coefficients in AMY:
    // f = const, note, vel, eg0, eg1, mod, bend
    // F = const, note, vel, eg0, eg1, mod, bend
    // a = const, note, vel, eg0, eg1, mod, bend
    // d = const, note, vel, eg0, eg1, mod, bend

    const p = window.synthStateManager ? window.synthStateManager.currentPatch : null;
    const baseCutoff = p ? p.filter_cutoff : 2500;
    const baseDuty = p ? p.duty : 0.5;

    // Coef order: 0:CONST, 1:NOTE, 2:VEL, 3:EG0, 4:EG1, 5:MOD, 6:BEND
    let f_coefs = [440.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0];
    let F_coefs = [baseCutoff, 0.3, 0.0, 0.0, 0.5, 0.0, 0.0];
    let a_coefs = [1.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0];
    let d_coefs = [baseDuty, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0];

    const mapSourceToIdx = (src) => {
      switch (src) {
        case 'vel': return 2;
        case 'eg0': return 3;
        case 'eg1': return 4;
        case 'lfo1':
        case 'modwheel': return 5;
        case 'pitchbend':
        case 'lfo2': return 6;
        default: return -1;
      }
    };

    for (const slot of this.slots) {
      if (slot.source === 'none' || slot.dest === 'none' || Math.abs(slot.amount) < 0.001) continue;
      const srcIdx = mapSourceToIdx(slot.source);
      if (srcIdx < 0) continue;

      const amt = slot.amount;

      switch (slot.dest) {
        case 'pitch':
          f_coefs[srcIdx] = amt * 2.0; // scale up to 2 octaves
          break;
        case 'cutoff':
          F_coefs[srcIdx] = amt * 2.0;
          break;
        case 'amp':
          a_coefs[srcIdx] = amt;
          break;
        case 'pwm':
          d_coefs[srcIdx] = amt * 0.45;
          break;
      }
    }

    const fmtArr = (arr) => arr.map(v => Number.isInteger(v) ? v : v.toFixed(3)).join(',');
    const wire = `i1f${fmtArr(f_coefs)}F${fmtArr(F_coefs)}a${fmtArr(a_coefs)}d${fmtArr(d_coefs)}Z`;
    return wire;
  }

  applyToEngine() {
    const wire = this.computeAmyWireCommands();
    if (window.amyAudioBridge) window.amyAudioBridge.sendWire(wire);
    if (window.esp32HardwareSync) window.esp32HardwareSync.sendWireIfConnected(wire);
  }

  renderUI() {
    if (!this.container) return;
    this.container.innerHTML = '';

    const grid = document.createElement('div');
    grid.className = 'mod-matrix-grid';
    grid.style.display = 'grid';
    grid.style.gridTemplateColumns = 'repeat(auto-fit, minmax(240px, 1fr))';
    grid.style.gap = '8px';

    this.slots.forEach((slot, idx) => {
      const card = document.createElement('div');
      card.className = 'mod-slot-card';
      card.style.background = '#0f141f';
      card.style.border = '1px solid #1e2638';
      card.style.borderRadius = '6px';
      card.style.padding = '8px';
      card.style.display = 'flex';
      card.style.flexDirection = 'column';
      card.style.gap = '6px';

      // Slot Title
      const header = document.createElement('div');
      header.style.display = 'flex';
      header.style.justifyContent = 'space-between';
      header.style.alignItems = 'center';
      header.innerHTML = `
        <span style="font-size: 10px; font-weight: 700; color: var(--accent-cyan);">ROTA #${idx + 1}</span>
        <span id="mod_amt_val_${idx}" style="font-size: 9px; font-family: monospace; color: ${slot.amount >= 0 ? '#00ff88' : '#ff3366'};">
          ${(slot.amount * 100).toFixed(0)}%
        </span>
      `;

      // Select Source
      const rowSrc = document.createElement('div');
      rowSrc.style.display = 'flex';
      rowSrc.style.justifyContent = 'space-between';
      rowSrc.style.alignItems = 'center';
      rowSrc.innerHTML = `<span style="font-size: 9px; color: #94a3b8;">FONTE</span>`;
      
      const selSrc = document.createElement('select');
      selSrc.className = 'patch-dropdown';
      selSrc.style.fontSize = '9px';
      selSrc.style.minWidth = '130px';
      this.sources.forEach(s => {
        const opt = document.createElement('option');
        opt.value = s.id;
        opt.innerText = s.label;
        if (s.id === slot.source) opt.selected = true;
        selSrc.appendChild(opt);
      });
      selSrc.addEventListener('change', (e) => this.setSlot(idx, e.target.value, undefined, undefined));
      rowSrc.appendChild(selSrc);

      // Select Destination
      const rowDest = document.createElement('div');
      rowDest.style.display = 'flex';
      rowDest.style.justifyContent = 'space-between';
      rowDest.style.alignItems = 'center';
      rowDest.innerHTML = `<span style="font-size: 9px; color: #94a3b8;">DESTINO</span>`;

      const selDest = document.createElement('select');
      selDest.className = 'patch-dropdown';
      selDest.style.fontSize = '9px';
      selDest.style.minWidth = '130px';
      this.destinations.forEach(d => {
        const opt = document.createElement('option');
        opt.value = d.id;
        opt.innerText = d.label;
        if (d.id === slot.dest) opt.selected = true;
        selDest.appendChild(opt);
      });
      selDest.addEventListener('change', (e) => this.setSlot(idx, undefined, e.target.value, undefined));
      rowDest.appendChild(selDest);

      // Bipolar Amount Slider (-1.0 to +1.0)
      const rowAmt = document.createElement('div');
      rowAmt.style.display = 'flex';
      rowAmt.style.alignItems = 'center';
      rowAmt.style.gap = '6px';

      const slider = document.createElement('input');
      slider.type = 'range';
      slider.min = '-100';
      slider.max = '100';
      slider.value = Math.round(slot.amount * 100);
      slider.className = 'juno-slider';
      slider.style.flex = '1';
      slider.style.height = '4px';

      slider.addEventListener('input', (e) => {
        const amt = parseFloat(e.target.value) / 100.0;
        this.setSlot(idx, undefined, undefined, amt);
        const readout = document.getElementById(`mod_amt_val_${idx}`);
        if (readout) {
          readout.innerText = `${(amt * 100).toFixed(0)}%`;
          readout.style.color = amt >= 0 ? '#00ff88' : '#ff3366';
        }
      });

      rowAmt.appendChild(slider);

      card.appendChild(header);
      card.appendChild(rowSrc);
      card.appendChild(rowDest);
      card.appendChild(rowAmt);
      grid.appendChild(card);
    });

    this.container.appendChild(grid);
  }

  exportData() {
    return this.slots.map(s => ({ ...s }));
  }

  importData(slotsData) {
    if (Array.isArray(slotsData)) {
      slotsData.forEach((s, idx) => {
        if (idx < this.numSlots) {
          this.slots[idx] = { ...s };
        }
      });
      this.renderUI();
      this.applyToEngine();
    }
  }
}

window.ModulationMatrixManager = ModulationMatrixManager;
