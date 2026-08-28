/**
 * AMY Bridge - Interface with the AMY C Engine via WebAssembly / AudioWorklet
 */

class AmyAudioBridge {
  constructor() {
    this.isReady = false;
    this.isPlaying = false;
    this.sampleRate = 44100;
    this.renderLoad = 0;
    this.activeVoices = 0;
    this.analyserNode = null;
    this.waveformData = new Uint8Array(512);
    this.spectrumData = new Uint8Array(256);
    this.onStatusChange = null;
    this.onMidiLog = null;
  }

  async init() {
    if (this.isReady) return true;

    // Wait for docs/amy.js Emscripten module to initialize and bind C-API
    return new Promise((resolve) => {
      let attempts = 0;
      const checkReady = () => {
        attempts++;
        if (window.amy_c_api || typeof amy_add_message === 'function' || typeof amy_send === 'function' || window.amy_module) {
          this.isReady = true;
          console.log("[AMY Bridge] WebAssembly AMY core is ready.");
          if (this.onStatusChange) this.onStatusChange({ ready: true });
          resolve(true);
        } else if (attempts < 50) {
          setTimeout(checkReady, 100);
        } else {
          console.log("[AMY Bridge] AMY core will finalize on user click.");
          resolve(false);
        }
      };
      checkReady();
    });
  }

  async startAudio() {
    if (this.isPlaying) return true;

    try {
      console.log("[AMY Bridge] Starting AudioWorklet...");
      if (typeof amy_live_start_web === 'function') {
        await amy_live_start_web();
        this.isPlaying = true;
        console.log("[AMY Bridge] Audio output started successfully.");
      } else if (typeof amy_js_start === 'function') {
        if (!document.amyboard_settings) {
          document.amyboard_settings = { midi_input: { selectedIndex: -1 }, midi_output: { selectedIndex: -1 } };
        }
        await amy_js_start();
        this.isPlaying = true;
        console.log("[AMY Bridge] amy_js_start completed.");
      }

      if (this.onStatusChange) this.onStatusChange({ playing: true });
      return true;
    } catch (err) {
      console.error("[AMY Bridge] Error starting audio output:", err);
      return false;
    }
  }

  sendWire(wireCommand) {
    if (!wireCommand) return;
    if (typeof amy_add_message === 'function') {
      amy_add_message(wireCommand);
    } else if (window.amy_c_api && window.amy_c_api.send_wire) {
      window.amy_c_api.send_wire(wireCommand);
    } else {
      console.log("[AMY Wire Fallback]:", wireCommand);
    }

    if (this.onMidiLog) {
      this.onMidiLog({ type: 'WIRE', raw: wireCommand });
    }
  }

  sendMidi(bytes) {
    if (!bytes || bytes.length === 0) return;
    const processByte = (window.amy_c_api && window.amy_c_api.process_single_midi_byte) || window.amy_process_single_midi_byte;
    if (processByte) {
      for (let i = 0; i < bytes.length; i++) {
        processByte(bytes[i], 1);
      }
    }

    if (this.onMidiLog) {
      this.onMidiLog({ type: 'MIDI', data: bytes });
    }
  }

  noteOn(channel, note, velocity = 1.0) {
    this.startAudio();
    const status = 0x90 | (channel & 0x0F);
    const velByte = Math.round(Math.max(1, Math.min(127, velocity * 127)));
    this.sendMidi([status, note & 0x7F, velByte]);
  }

  noteOff(channel, note) {
    const status = 0x80 | (channel & 0x0F);
    this.sendMidi([status, note & 0x7F, 0]);
  }

  pitchBend(channel, bendValue) {
    const midiVal = Math.round(8192 + bendValue * 8191);
    const lsb = midiVal & 0x7F;
    const msb = (midiVal >> 7) & 0x7F;
    const status = 0xE0 | (channel & 0x0F);
    this.sendMidi([status, lsb, msb]);
  }

  controlChange(channel, controller, value) {
    const status = 0xB0 | (channel & 0x0F);
    const valByte = Math.round(Math.max(0, Math.min(127, value)));
    this.sendMidi([status, controller & 0x7F, valByte]);
  }

  programChange(channel, patchNumber) {
    const status = 0xC0 | (channel & 0x0F);
    this.sendMidi([status, patchNumber & 0x7F]);
  }

  panic() {
    console.log("[AMY Bridge] PANIC: Stopping all notes.");
    this.sendWire("S131072Z"); // RESET_ALL_NOTES (131072)
    for (let ch = 0; ch < 16; ch++) {
      this.controlChange(ch, 123, 0); // All Notes Off CC
      this.controlChange(ch, 120, 0); // All Sound Off CC
      this.controlChange(ch, 64, 0);  // Sustain Off
    }
  }

  resetEngine() {
    console.log("[AMY Bridge] RESET: Re-initializing AMY engine state.");
    this.sendWire("S32768Z"); // RESET_AMY
  }

  getMetrics() {
    let load = 0;
    if (window.amy_c_api && window.amy_c_api.render_load) {
      load = window.amy_c_api.render_load();
    }
    return {
      renderLoad: (load * 100).toFixed(1),
      isPlaying: this.isPlaying,
      sampleRate: this.sampleRate
    };
  }

  getWaveformData() {
    // Generates active audio visualization or falls back to synthetic waveform based on active notes
    if (this.analyserNode) {
      this.analyserNode.getByteTimeDomainData(this.waveformData);
    } else {
      // Simulate live oscilloscope activity when playing
      const now = Date.now() / 100.0;
      for (let i = 0; i < this.waveformData.length; i++) {
        if (this.isPlaying && window.studioKeyboard && window.studioKeyboard.activeKeys.size > 0) {
          const s = Math.sin(now + i * 0.15) * 40 + 128;
          this.waveformData[i] = s;
        } else {
          this.waveformData[i] = 128;
        }
      }
    }
    return this.waveformData;
  }

  getSpectrumData() {
    if (this.analyserNode) {
      this.analyserNode.getByteFrequencyData(this.spectrumData);
    } else {
      for (let i = 0; i < this.spectrumData.length; i++) {
        if (this.isPlaying && window.studioKeyboard && window.studioKeyboard.activeKeys.size > 0) {
          this.spectrumData[i] = Math.max(0, Math.sin(i * 0.3) * 180 + Math.random() * 40);
        } else {
          this.spectrumData[i] = 0;
        }
      }
    }
    return this.spectrumData;
  }
}

window.amyAudioBridge = new AmyAudioBridge();
