/**
 * AMY Studio - Interactive Piano Keyboard & Computer QWERTY Controller
 */

class StudioKeyboardController {
  constructor() {
    this.octave = 4; // C4 = MIDI note 60
    this.velocity = 0.85;
    this.activeKeys = new Set();
    this.pitchBendVal = 0.0;
    this.modWheelVal = 0.0;
    
    // QWERTY Key mappings to semitones from current octave root C
    this.keyMap = {
      'KeyZ': 0,  // C
      'KeyS': 1,  // C#
      'KeyX': 2,  // D
      'KeyD': 3,  // D#
      'KeyC': 4,  // E
      'KeyV': 5,  // F
      'KeyG': 6,  // F#
      'KeyB': 7,  // G
      'KeyH': 8,  // G#
      'KeyN': 9,  // A
      'KeyJ': 10, // A#
      'KeyM': 11, // B
      'Comma': 12,// C (next)
      'KeyQ': 12, // C
      'Digit2': 13,// C#
      'KeyW': 14, // D
      'Digit3': 15,// D#
      'KeyE': 16, // E
      'KeyR': 17, // F
      'Digit5': 18,// F#
      'KeyT': 19, // G
      'Digit6': 20,// G#
      'KeyY': 21, // A
      'Digit7': 22,// A#
      'KeyU': 23, // B
      'KeyI': 24  // C
    };
  }

  init(containerId) {
    this.renderKeys(containerId);
    this.setupEventListeners();
  }

  renderKeys(containerId) {
    const container = document.getElementById(containerId);
    if (!container) return;
    container.innerHTML = '';

    const pianoKeys = document.createElement('div');
    pianoKeys.className = 'piano-keys';

    // 25 Keys (2 full octaves + 1 note: C to C)
    const baseMidi = this.octave * 12;
    const numKeys = 25;

    for (let i = 0; i < numKeys; i++) {
      const midiNote = baseMidi + i;
      const noteInOct = i % 12;
      const isBlack = [1, 3, 6, 8, 10].includes(noteInOct);

      const keyEl = document.createElement('div');
      keyEl.dataset.note = midiNote;

      if (!isBlack) {
        keyEl.className = 'key-white';
        keyEl.id = `key_${midiNote}`;
      } else {
        keyEl.className = 'key-black';
        keyEl.id = `key_${midiNote}`;
        
        // Calculate relative black key offset
        const whiteIndicesBefore = [0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6];
        const whiteCountInOct = 7;
        const octNum = Math.floor(i / 12);
        const whitePos = (octNum * whiteCountInOct) + whiteIndicesBefore[noteInOct];
        const whiteKeyWidthPercent = 100 / 15; // 15 white keys in 25 keys
        keyEl.style.left = `${(whitePos + 0.68) * whiteKeyWidthPercent}%`;
      }

      keyEl.addEventListener('mousedown', (e) => {
        e.preventDefault();
        this.triggerNoteOn(midiNote);
      });

      keyEl.addEventListener('mouseup', () => this.triggerNoteOff(midiNote));
      keyEl.addEventListener('mouseleave', () => this.triggerNoteOff(midiNote));

      pianoKeys.appendChild(keyEl);
    }

    container.appendChild(pianoKeys);
    if (window.layerSplitManager) window.layerSplitManager.updateKeyboardVisuals();
  }

  setupEventListeners() {
    window.addEventListener('keydown', (e) => {
      if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;
      if (e.repeat) return;

      if (e.code === 'Minus' || e.code === 'BracketLeft') {
        this.octaveDown();
        return;
      }
      if (e.code === 'Equal' || e.code === 'BracketRight') {
        this.octaveUp();
        return;
      }

      if (this.keyMap.hasOwnProperty(e.code)) {
        const offset = this.keyMap[e.code];
        const note = (this.octave * 12) + offset;
        this.triggerNoteOn(note);
      }
    });

    window.addEventListener('keyup', (e) => {
      if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;

      if (this.keyMap.hasOwnProperty(e.code)) {
        const offset = this.keyMap[e.code];
        const note = (this.octave * 12) + offset;
        this.triggerNoteOff(note);
      }
    });
  }

  triggerNoteOn(note) {
    if (this.activeKeys.has(note)) return;
    this.activeKeys.add(note);

    const el = document.getElementById(`key_${note}`);
    if (el) el.classList.add('active');

    if (window.layerSplitManager) {
      window.layerSplitManager.routeNoteOn(0, note, this.velocity);
    } else {
      // Send to local AMY Audio Worklet
      if (window.amyAudioBridge) {
        window.amyAudioBridge.noteOn(0, note, this.velocity);
      }

      // Send to connected ESP32-S3 hardware
      if (window.esp32HardwareSync) {
        window.esp32HardwareSync.noteOn(0, note, this.velocity);
      }
    }
  }

  triggerNoteOff(note) {
    if (!this.activeKeys.has(note)) return;
    this.activeKeys.delete(note);

    const el = document.getElementById(`key_${note}`);
    if (el) el.classList.remove('active');

    if (window.layerSplitManager) {
      window.layerSplitManager.routeNoteOff(0, note);
    } else {
      if (window.amyAudioBridge) {
        window.amyAudioBridge.noteOff(0, note);
      }

      if (window.esp32HardwareSync) {
        window.esp32HardwareSync.noteOff(0, note);
      }
    }
  }

  octaveUp() {
    if (this.octave < 7) {
      this.octave++;
      this.renderKeys('pianoKeyboardContainer');
      const el = document.getElementById('octaveDisplay');
      if (el) el.innerText = `OCT C${this.octave}`;
    }
  }

  octaveDown() {
    if (this.octave > 1) {
      this.octave--;
      this.renderKeys('pianoKeyboardContainer');
      const el = document.getElementById('octaveDisplay');
      if (el) el.innerText = `OCT C${this.octave}`;
    }
  }

  setPitchBend(normVal) {
    // normVal: -1.0 to +1.0
    this.pitchBendVal = normVal;
    if (window.amyAudioBridge) window.amyAudioBridge.pitchBend(0, normVal);
    if (window.esp32HardwareSync) window.esp32HardwareSync.pitchBend(0, normVal);
  }

  setModWheel(normVal) {
    // normVal: 0.0 to 1.0 -> CC1 (0..127)
    this.modWheelVal = normVal;
    const midiVal = Math.round(normVal * 127);
    if (window.amyAudioBridge) window.amyAudioBridge.controlChange(0, 1, midiVal);
    if (window.esp32HardwareSync) window.esp32HardwareSync.controlChange(0, 1, midiVal);
  }
}

window.studioKeyboard = new StudioKeyboardController();
