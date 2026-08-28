/**
 * AMY Studio - Interactive Melodic Piano Roll Step Sequencer
 * 16-step polyphonic note grid with pitch, velocity, gate length, ratcheting and probability.
 */

class MelodicPianoRoll {
  constructor(containerId) {
    this.container = document.getElementById(containerId);
    this.numSteps = 16;
    this.baseNote = 48; // C3
    this.numNotes = 24; // 2 Octaves (C3 to B4)
    this.noteNames = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
    
    // 16 steps array: each step can have an active note object { note, vel, gate, ratchet, prob }
    this.steps = new Array(this.numSteps).fill(null);
    
    // Default initial test melody (Bass / Arp line)
    this.steps[0]  = { note: 48, vel: 0.9, gate: 0.8, ratchet: 1, prob: 1.0 };
    this.steps[2]  = { note: 51, vel: 0.8, gate: 0.8, ratchet: 1, prob: 1.0 };
    this.steps[4]  = { note: 55, vel: 0.85, gate: 0.8, ratchet: 1, prob: 1.0 };
    this.steps[6]  = { note: 58, vel: 0.8, gate: 0.8, ratchet: 1, prob: 1.0 };
    this.steps[8]  = { note: 60, vel: 1.0, gate: 0.8, ratchet: 1, prob: 1.0 };
    this.steps[10] = { note: 58, vel: 0.8, gate: 0.8, ratchet: 1, prob: 1.0 };
    this.steps[12] = { note: 55, vel: 0.85, gate: 0.8, ratchet: 1, prob: 1.0 };
    this.steps[14] = { note: 51, vel: 0.8, gate: 0.8, ratchet: 2, prob: 1.0 };

    if (this.container) {
      this.render();
    }
  }

  getNoteName(midiNote) {
    const oct = Math.floor(midiNote / 12) - 1;
    const name = this.noteNames[midiNote % 12];
    return `${name}${oct}`;
  }

  isBlackKey(midiNote) {
    const n = midiNote % 12;
    return [1, 3, 6, 8, 10].includes(n);
  }

  render() {
    if (!this.container) return;
    this.container.innerHTML = '';

    const grid = document.createElement('div');
    grid.className = 'piano-roll-grid';
    grid.style.display = 'grid';
    grid.style.gridTemplateColumns = `60px repeat(${this.numSteps}, 1fr)`;
    grid.style.gap = '2px';
    grid.style.background = '#0a0d14';
    grid.style.padding = '8px';
    grid.style.borderRadius = '8px';
    grid.style.border = '1px solid #1c2436';
    grid.style.maxHeight = '280px';
    grid.style.overflowY = 'auto';

    // Render from highest note down to lowest note
    for (let row = this.numNotes - 1; row >= 0; row--) {
      const midiNote = this.baseNote + row;
      const isBlack = this.isBlackKey(midiNote);

      // Pitch Key Header
      const keyHeader = document.createElement('div');
      keyHeader.className = `piano-roll-key ${isBlack ? 'black' : 'white'}`;
      keyHeader.innerText = this.getNoteName(midiNote);
      keyHeader.style.fontSize = '9px';
      keyHeader.style.fontFamily = 'JetBrains Mono, monospace';
      keyHeader.style.padding = '3px 6px';
      keyHeader.style.color = isBlack ? '#ffb700' : '#cbd5e1';
      keyHeader.style.background = isBlack ? '#121622' : '#1c2333';
      keyHeader.style.borderRight = '2px solid #334155';
      keyHeader.style.display = 'flex';
      keyHeader.style.alignItems = 'center';
      keyHeader.style.cursor = 'pointer';

      keyHeader.addEventListener('mousedown', () => {
        if (window.amyAudioBridge) window.amyAudioBridge.noteOn(0, midiNote, 0.8);
      });
      keyHeader.addEventListener('mouseup', () => {
        if (window.amyAudioBridge) window.amyAudioBridge.noteOff(0, midiNote);
      });

      grid.appendChild(keyHeader);

      // 16 Step Cells for this Pitch
      for (let s = 0; s < this.numSteps; s++) {
        const cell = document.createElement('div');
        cell.className = 'piano-roll-cell';
        cell.id = `pr_cell_${midiNote}_${s}`;
        cell.style.height = '14px';
        cell.style.background = isBlack ? '#0d111a' : '#141a27';
        cell.style.border = '1px solid #1e2638';
        cell.style.borderRadius = '2px';
        cell.style.cursor = 'pointer';

        if (s % 4 === 0) {
          cell.style.borderLeft = '2px solid #3b4666';
        }

        const stepData = this.steps[s];
        if (stepData && stepData.note === midiNote) {
          cell.style.background = 'linear-gradient(135deg, #00f0ff, #0077ff)';
          cell.style.boxShadow = '0 0 6px rgba(0, 240, 255, 0.5)';
        }

        cell.addEventListener('click', () => {
          if (this.steps[s] && this.steps[s].note === midiNote) {
            this.steps[s] = null; // Toggle Off
          } else {
            this.steps[s] = { note: midiNote, vel: 0.85, gate: 0.8, ratchet: 1, prob: 1.0 }; // Toggle On
          }
          this.render();
        });

        grid.appendChild(cell);
      }
    }

    this.container.appendChild(grid);
  }

  setPlayhead(stepIndex) {
    document.querySelectorAll('.piano-roll-cell').forEach(c => c.style.outline = 'none');
    for (let row = 0; row < this.numNotes; row++) {
      const midiNote = this.baseNote + row;
      const el = document.getElementById(`pr_cell_${midiNote}_${stepIndex}`);
      if (el) el.style.outline = '2px solid #ffb700';
    }
  }
}

window.MelodicPianoRoll = MelodicPianoRoll;
