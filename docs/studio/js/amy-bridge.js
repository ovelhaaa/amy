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
    this.audioContext = null;
    this.analyserNode = null;
    this.waveformData = new Uint8Array(512);
    this.spectrumData = new Uint8Array(256);
    this.onStatusChange = null;
    this.onMidiLog = null;
  }

  async init() {
    if (this.isReady) return true;

    try {
      console.log("[AMY Bridge] Initializing WebAssembly AMY core...");
      // Check if global amy_c_api is already bound
      if (typeof window !== 'undefined' && window.amy_c_api) {
        this.isReady = true;
        console.log("[AMY Bridge] AMY WebAssembly C-API bound successfully.");
        if (this.onStatusChange) this.onStatusChange({ ready: true });
        return true;
      }

      // If amyModule exists, wait for it
      if (typeof amyModule === 'function') {
        const am = await amyModule();
        if (typeof amy_c_api_bind === 'function') {
          window.amy_c_api = amy_c_api_bind(am);
          this.isReady = true;
          console.log("[AMY Bridge] AMY Module loaded and bound.");
          if (this.onStatusChange) this.onStatusChange({ ready: true });
          return true;
        }
      }

      console.warn("[AMY Bridge] Waiting for docs/amy.js to load completely...");
      return false;
    } catch (err) {
      console.error("[AMY Bridge] Initialization error:", err);
      return false;
    }
  }

  async startAudio() {
    if (this.isPlaying) return true;

    try {
      if (typeof amy_js_start === 'function') {
        await amy_js_start();
        this.isPlaying = true;
        console.log("[AMY Bridge] Audio output started successfully.");

        // Setup WebAudio Analyser if AudioContext is accessible
        if (window.AudioContext || window.webkitAudioContext) {
          try {
            // Miniaudio creates an audio context or we hook the worklet
            const ctx = window.amy_audio_context || new (window.AudioContext || window.webkitAudioContext)();
            this.audioContext = ctx;
            this.analyserNode = ctx.createAnalyser();
            this.analyserNode.fftSize = 1024;
            this.waveformData = new Uint8Array(this.analyserNode.frequencyBinCount);
            this.spectrumData = new Uint8Array(this.analyserNode.frequencyBinCount);
          } catch (e) {
            console.log("[AMY Bridge] Visualizer analyser fallback active:", e);
          }
        }

        if (this.onStatusChange) this.onStatusChange({ playing: true });
        return true;
      }
    } catch (err) {
      console.error("[AMY Bridge] Error starting audio output:", err);
      return false;
    }
    return false;
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
    // Convert to canonical AMY wire command or MIDI Note On
    // Status byte 0x90 + channel (0..15)
    const status = 0x90 | (channel & 0x0F);
    const velByte = Math.round(Math.max(1, Math.min(127, velocity * 127)));
    this.sendMidi([status, note & 0x7F, velByte]);
  }

  noteOff(channel, note) {
    // MIDI Note Off: status byte 0x80 + channel
    const status = 0x80 | (channel & 0x0F);
    this.sendMidi([status, note & 0x7F, 0]);
  }

  pitchBend(channel, bendValue) {
    // bendValue: -1.0 to +1.0 -> 0 to 16383 (center = 8192)
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
    console.log("[AMY Bridge] PANIC: Stopping all notes and resetting audio voices.");
    // Send AMY Reset All Notes + All Notes Off
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
    if (this.analyserNode) {
      this.analyserNode.getByteTimeDomainData(this.waveformData);
    }
    return this.waveformData;
  }

  getSpectrumData() {
    if (this.analyserNode) {
      this.analyserNode.getByteFrequencyData(this.spectrumData);
    }
    return this.spectrumData;
  }
}

window.amyAudioBridge = new AmyAudioBridge();
