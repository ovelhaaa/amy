/**
 * AMY Studio - Drum Machine, Melodic Step Sequencer & Arpeggiator
 */

class AmyStudioSequencer {
  constructor() {
    this.bpm = 120.0;
    this.isPlaying = false;
    this.currentStep = 0;
    this.totalSteps = 16;
    this.swing = 0.50; // 50% = straight, 66% = triplet/shuffle
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

    // Melodic Synth Track (16 steps)
    this.synthTrack = [
      { active: 1, note: 48, vel: 100, gate: 0.7 }, // C3
      { active: 0, note: 48, vel: 90,  gate: 0.7 },
      { active: 1, note: 51, vel: 95,  gate: 0.7 }, // Eb3
      { active: 0, note: 48, vel: 80,  gate: 0.7 },
      { active: 1, note: 55, vel: 110, gate: 0.7 }, // G3
      { active: 0, note: 48, vel: 80,  gate: 0.7 },
      { active: 1, note: 58, vel: 100, gate: 0.7 }, // Bb3
      { active: 1, note: 60, vel: 120, gate: 0.7 }, // C4
      { active: 1, note: 48, vel: 100, gate: 0.7 },
      { active: 0, note: 48, vel: 80,  gate: 0.7 },
      { active: 1, note: 58, vel: 90,  gate: 0.7 },
      { active: 0, note: 48, vel: 80,  gate: 0.7 },
      { active: 1, note: 55, vel: 105, gate: 0.7 },
      { active: 1, note: 53, vel: 95,  gate: 0.7 }, // F3
      { active: 1, note: 51, vel: 100, gate: 0.7 },
      { active: 0, note: 48, vel: 80,  gate: 0.7 }
    ];

    // Arpeggiator State
    this.arpEnabled = false;
    this.arpMode = "UP"; // UP, DOWN, UPDOWN, RANDOM, CHORD
    this.arpDivision = 16; // 1/16th notes
    this.arpOctaves = 2;
    this.arpLatch = false;
    this.heldNotes = [];
    this.arpIndex = 0;
    this.onStepChange = null;
  }

  setBpm(newBpm) {
    this.bpm = Math.max(30, Math.min(300, newBpm));
    if (this.isPlaying) {
      this.stop();
      this.play();
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
      }
    }

    // 2. Trigger Melodic Synth Track Step
    const sStep = this.synthTrack[step];
    if (sStep && sStep.active) {
      if (window.amyAudioBridge) {
        window.amyAudioBridge.noteOn(0, sStep.note, sStep.vel / 127.0);
        setTimeout(() => {
          window.amyAudioBridge.noteOff(0, sStep.note);
        }, ((60000.0 / this.bpm) / 4.0) * sStep.gate);
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

  triggerArpStep() {
    let sortedNotes = [...this.heldNotes].sort((a, b) => a - b);
    let allNotes = [];
    for (let oct = 0; oct < this.arpOctaves; oct++) {
      for (let n of sortedNotes) {
        allNotes.push(n + oct * 12);
      }
    }

    let noteToPlay = allNotes[0];
    if (this.arpMode === "UP") {
      noteToPlay = allNotes[this.arpIndex % allNotes.length];
      this.arpIndex++;
    } else if (this.arpMode === "DOWN") {
      noteToPlay = allNotes[(allNotes.length - 1 - (this.arpIndex % allNotes.length))];
      this.arpIndex++;
    } else if (this.arpMode === "RANDOM") {
      noteToPlay = allNotes[Math.floor(Math.random() * allNotes.length)];
    }

    if (window.amyAudioBridge) {
      window.amyAudioBridge.noteOn(0, noteToPlay, 0.85);
      setTimeout(() => {
        window.amyAudioBridge.noteOff(0, noteToPlay);
      }, ((60000.0 / this.bpm) / 4.0) * 0.7);
    }
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
