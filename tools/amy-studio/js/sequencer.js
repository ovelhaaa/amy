/**
 * AMY Studio - Drum Machine, Melodic Step Sequencer & Arpeggiator
 * Features Live Step Recording (Quantized Input), Extended Arpeggiator (AsPlayed, Chord, Triplets, Swing, Latch),
 * and bidirectional hardware synchronization with the ESP32-S3 firmware.
 */

class AmyStudioSequencer {
  constructor() {
    this.bpm = 120.0;
    this.isPlaying = false;
    this.isRecording = false;
    this.currentStep = 0;
    this.totalSteps = 16;
    this.swing = 50; // 50% = straight, 66% = triplet/shuffle, up to 75%
    this.clockTimer = null;
    
    // Drum Tracks (8 tracks, 16 steps each)
    this.drumTracks = [
      { name: "KICK",  note: 36, preset: 385, pattern: [1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0] },
      { name: "SNARE", note: 38, preset: 385, pattern: [0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0] },
      { name: "CH_HAT",note: 42, preset: 385, pattern: [1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1] },
      { name: "OH_HAT",note: 46, preset: 385, pattern: [0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,0] },
      { name: "CLAP",  note: 39, preset: 385, pattern: [0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0] },
      { name: "TOM",   note: 45, preset: 385, pattern: [0,0,0,0, 0,0,1,0, 0,0,0,0, 0,1,0,0] },
      { name: "RIM",   note: 37, preset: 385, pattern: [0,0,0,1, 0,0,0,1, 0,0,0,1, 0,0,0,1] },
      { name: "CRASH", note: 49, preset: 385, pattern: [1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0] }
    ];

    // Melodic Synth Track (16 steps fallback)
    this.synthTrack = [
      { active: 1, note: 48, vel: 100, gate: 0.7 },
      { active: 0, note: 48, vel: 90,  gate: 0.7 },
      { active: 1, note: 51, vel: 95,  gate: 0.7 },
      { active: 0, note: 48, vel: 80,  gate: 0.7 },
      { active: 1, note: 55, vel: 110, gate: 0.7 },
      { active: 0, note: 48, vel: 80,  gate: 0.7 },
      { active: 1, note: 58, vel: 100, gate: 0.7 },
      { active: 1, note: 60, vel: 120, gate: 0.7 },
      { active: 1, note: 48, vel: 100, gate: 0.7 },
      { active: 0, note: 48, vel: 80,  gate: 0.7 },
      { active: 1, note: 58, vel: 90,  gate: 0.7 },
      { active: 0, note: 48, vel: 80,  gate: 0.7 },
      { active: 1, note: 55, vel: 105, gate: 0.7 },
      { active: 1, note: 53, vel: 95,  gate: 0.7 },
      { active: 1, note: 51, vel: 100, gate: 0.7 },
      { active: 0, note: 48, vel: 80,  gate: 0.7 }
    ];

    // Arpeggiator State
    this.arpEnabled = false;
    this.arpMode = "UP"; // UP, DOWN, UPDOWN, RANDOM, ASPLAYED, CHORD
    this.arpDivision = "1/16"; // 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32
    this.arpOctaves = 2;
    this.arpLatch = false;
    this.heldNotes = []; // Array of { note, time }
    this.arpIndex = 0;
    this.arpDirectionUp = true;
    this.onStepChange = null;
    this.onRecordStateChange = null;
  }

  setBpm(newBpm) {
    this.bpm = Math.max(30, Math.min(300, newBpm));
    if (this.isPlaying) {
      this.stop();
      this.play();
    }
  }

  toggleRecord() {
    this.isRecording = !this.isRecording;
    console.log(`[Sequencer] Live Step Recording: ${this.isRecording ? 'ON' : 'OFF'}`);
    if (this.onRecordStateChange) this.onRecordStateChange(this.isRecording);
    if (this.isRecording && !this.isPlaying) {
      this.play();
    }
  }

  recordLiveNote(note, velocity = 0.85) {
    if (!this.isRecording) return;
    const step = this.currentStep;

    // 1. Check if it matches a Drum Track note
    const dTrackIdx = this.drumTracks.findIndex(t => t.note === note);
    if (dTrackIdx !== -1) {
      this.drumTracks[dTrackIdx].pattern[step] = 1;
      const el = document.getElementById(`drum_step_${dTrackIdx}_${step}`);
      if (el) {
        el.classList.add('active');
        el.innerText = '●';
      }
      console.log(`[Live Record] Drum Hit: Track ${this.drumTracks[dTrackIdx].name} at step ${step + 1}`);
      return;
    }

    // 2. Otherwise record into Melodic Piano Roll
    if (window.melodicPianoRoll) {
      window.melodicPianoRoll.steps[step] = {
        note: note,
        vel: velocity,
        gate: 0.8,
        ratchet: 1,
        prob: 1.0
      };
      window.melodicPianoRoll.render();
      window.melodicPianoRoll.setPlayhead(step);
      console.log(`[Live Record] Melodic Note: ${note} at step ${step + 1}`);
    }
  }

  play() {
    if (this.isPlaying) return;
    this.isPlaying = true;
    this.currentStep = 0;
    if (window.amyAudioBridge) window.amyAudioBridge.startAudio();

    const intervalMs = (60000.0 / this.bpm) / 4.0; // 16th notes
    this.clockTimer = setInterval(() => this.tick(), intervalMs);
    console.log(`[Sequencer] Started at ${this.bpm} BPM.`);
  }

  stop() {
    this.isPlaying = false;
    this.isRecording = false;
    if (this.onRecordStateChange) this.onRecordStateChange(false);
    if (this.clockTimer) {
      clearInterval(this.clockTimer);
      this.clockTimer = null;
    }
    if (window.amyAudioBridge) window.amyAudioBridge.panic();
    console.log("[Sequencer] Stopped.");
  }

  toggle() {
    if (this.isPlaying) this.stop();
    else this.play();
  }

  tick() {
    const step = this.currentStep;

    // 1. Trigger Active Drum Steps
    for (let t = 0; t < this.drumTracks.length; t++) {
      const track = this.drumTracks[t];
      if (track.pattern[step] === 1) {
        // AMY channel 9 (MIDI ch 10 = drums)
        if (window.amyAudioBridge) {
          window.amyAudioBridge.noteOn(9, track.note, 0.9);
        }
        if (window.esp32HardwareSync) {
          window.esp32HardwareSync.noteOn(9, track.note, 0.9);
        }
      }
    }

    // 2. Trigger Melodic Synth Track Step (from Piano Roll if available)
    if (window.melodicPianoRoll && window.melodicPianoRoll.steps) {
      const pStep = window.melodicPianoRoll.steps[step];
      if (pStep && pStep.note) {
        if (Math.random() <= pStep.prob) { // probability check
          const gateDur = ((60000.0 / this.bpm) / 4.0) * pStep.gate;
          if (window.amyAudioBridge) window.amyAudioBridge.noteOn(0, pStep.note, pStep.vel);
          if (window.esp32HardwareSync) window.esp32HardwareSync.noteOn(0, pStep.note, pStep.vel);
          setTimeout(() => {
            if (window.amyAudioBridge) window.amyAudioBridge.noteOff(0, pStep.note);
            if (window.esp32HardwareSync) window.esp32HardwareSync.noteOff(0, pStep.note);
          }, gateDur);
        }
      }
    } else {
      const sStep = this.synthTrack[step];
      if (sStep && sStep.active) {
        const velNorm = sStep.vel / 127.0;
        const gateDur = ((60000.0 / this.bpm) / 4.0) * sStep.gate;
        if (window.amyAudioBridge) window.amyAudioBridge.noteOn(0, sStep.note, velNorm);
        if (window.esp32HardwareSync) window.esp32HardwareSync.noteOn(0, sStep.note, velNorm);
        setTimeout(() => {
          if (window.amyAudioBridge) window.amyAudioBridge.noteOff(0, sStep.note);
          if (window.esp32HardwareSync) window.esp32HardwareSync.noteOff(0, sStep.note);
        }, gateDur);
      }
    }

    // 3. Trigger Arpeggiator Note (if enabled and notes held)
    if (this.arpEnabled && this.heldNotes.length > 0) {
      this.triggerArpStep();
    }

    // Advance step
    this.currentStep = (this.currentStep + 1) % this.totalSteps;
    if (this.onStepChange) this.onStepChange(step);
  }

  addHeldNote(note) {
    if (!this.heldNotes.some(h => h.note === note)) {
      this.heldNotes.push({ note, time: Date.now() });
    }
  }

  removeHeldNote(note) {
    if (!this.arpLatch) {
      this.heldNotes = this.heldNotes.filter(h => h.note !== note);
    }
  }

  setArpMode(mode) {
    this.arpMode = mode;
    this.arpIndex = 0;
    this.arpDirectionUp = true;
    if (window.esp32HardwareSync) {
      window.esp32HardwareSync.sendSerialCommand(`arp_mode ${mode.toLowerCase()}`);
    }
  }

  setArpEnabled(enable) {
    this.arpEnabled = enable;
    if (!enable && !this.arpLatch) this.heldNotes = [];
    if (window.esp32HardwareSync) {
      window.esp32HardwareSync.sendSerialCommand(`arp_enable ${enable ? 1 : 0}`);
    }
  }

  setArpDivision(div) {
    this.arpDivision = div;
  }

  setArpOctaves(oct) {
    this.arpOctaves = Math.max(1, Math.min(4, oct));
  }

  setArpLatch(latch) {
    this.arpLatch = latch;
    if (!latch) {
      this.heldNotes = [];
    }
  }

  setArpSwing(swingPct) {
    this.swing = Math.max(50, Math.min(75, swingPct));
    if (window.esp32HardwareSync) {
      window.esp32HardwareSync.sendSerialCommand(`arp_swing ${this.swing}`);
    }
  }

  triggerArpStep() {
    if (this.heldNotes.length === 0) return;

    let baseNotes = [];
    if (this.arpMode === "ASPLAYED") {
      // Insertion order (chronological order keys were pressed)
      baseNotes = this.heldNotes.map(h => h.note);
    } else {
      // Pitch-sorted order
      baseNotes = [...this.heldNotes.map(h => h.note)].sort((a, b) => a - b);
    }

    let allNotes = [];
    for (let oct = 0; oct < this.arpOctaves; oct++) {
      for (let n of baseNotes) {
        allNotes.push(n + oct * 12);
      }
    }

    const arpGate = ((60000.0 / this.bpm) / 4.0) * 0.75;

    // CHORD Mode (Stutter / Gated simultaneous chord)
    if (this.arpMode === "CHORD") {
      for (let n of allNotes) {
        if (window.amyAudioBridge) window.amyAudioBridge.noteOn(0, n, 0.85);
        if (window.esp32HardwareSync) window.esp32HardwareSync.noteOn(0, n, 0.85);
        setTimeout(() => {
          if (window.amyAudioBridge) window.amyAudioBridge.noteOff(0, n);
          if (window.esp32HardwareSync) window.esp32HardwareSync.noteOff(0, n);
        }, arpGate);
      }
      return;
    }

    // Melodic Sequential Modes
    let noteToPlay = allNotes[0];
    if (this.arpMode === "UP") {
      noteToPlay = allNotes[this.arpIndex % allNotes.length];
      this.arpIndex++;
    } else if (this.arpMode === "DOWN") {
      noteToPlay = allNotes[(allNotes.length - 1 - (this.arpIndex % allNotes.length))];
      this.arpIndex++;
    } else if (this.arpMode === "UPDOWN") {
      // Pendulum up & down
      if (allNotes.length === 1) {
        noteToPlay = allNotes[0];
      } else {
        noteToPlay = allNotes[this.arpIndex];
        if (this.arpDirectionUp) {
          this.arpIndex++;
          if (this.arpIndex >= allNotes.length - 1) {
            this.arpDirectionUp = false;
          }
        } else {
          this.arpIndex--;
          if (this.arpIndex <= 0) {
            this.arpDirectionUp = true;
          }
        }
      }
    } else if (this.arpMode === "RANDOM") {
      noteToPlay = allNotes[Math.floor(Math.random() * allNotes.length)];
    } else if (this.arpMode === "ASPLAYED") {
      noteToPlay = allNotes[this.arpIndex % allNotes.length];
      this.arpIndex++;
    }

    if (window.amyAudioBridge) window.amyAudioBridge.noteOn(0, noteToPlay, 0.85);
    if (window.esp32HardwareSync) window.esp32HardwareSync.noteOn(0, noteToPlay, 0.85);
    setTimeout(() => {
      if (window.amyAudioBridge) window.amyAudioBridge.noteOff(0, noteToPlay);
      if (window.esp32HardwareSync) window.esp32HardwareSync.noteOff(0, noteToPlay);
    }, arpGate);
  }

  toggleDrumStep(trackIdx, stepIdx) {
    if (trackIdx >= 0 && trackIdx < this.drumTracks.length) {
      const cur = this.drumTracks[trackIdx].pattern[stepIdx];
      this.drumTracks[trackIdx].pattern[stepIdx] = cur === 1 ? 0 : 1;
    }
  }

  toggleSynthStep(stepIdx) {
    if (stepIdx >= 0 && stepIdx < this.synthTrack.length) {
      this.synthTrack[stepIdx].active = this.synthTrack[stepIdx].active ? 0 : 1;
    }
  }

  setSynthStepNote(stepIdx, note) {
    if (stepIdx >= 0 && stepIdx < this.synthTrack.length) {
      this.synthTrack[stepIdx].note = note;
    }
  }
}

window.amyStudioSequencer = new AmyStudioSequencer();
