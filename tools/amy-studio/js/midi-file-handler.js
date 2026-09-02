/**
 * AMY Studio - Standard MIDI File (SMF Type 0) Encoder & Decoder
 * Provides pure JavaScript MIDI file generation and parsing for DAWs (Ableton, FL Studio, Logic, Reaper)
 * supporting 16-step polyphonic melodic piano roll and 8-track drum machine channels.
 */

class StandardMidiFileHandler {
  constructor() {
    this.ppqn = 480; // Standard 480 pulses per quarter note (16th note = 120 ticks)
  }

  // ══════════════════════════════════════════════════════════
  // VARIABLE LENGTH QUANTITY (VLQ) HELPERS
  // ══════════════════════════════════════════════════════════
  encodeVLQ(value) {
    let buffer = [];
    let v = value;
    buffer.push(v & 0x7F);
    while ((v >>= 7)) {
      buffer.push((v & 0x7F) | 0x80);
    }
    return buffer.reverse();
  }

  decodeVLQ(bytes, offset) {
    let value = 0;
    let bytesRead = 0;
    let byte;
    do {
      byte = bytes[offset + bytesRead];
      value = (value << 7) | (byte & 0x7F);
      bytesRead++;
    } while (byte & 0x80);
    return { value, bytesRead };
  }

  // ══════════════════════════════════════════════════════════
  // MIDI FILE GENERATION (SMF TYPE 0)
  // ══════════════════════════════════════════════════════════
  exportMidiFile(drumTracks, pianoRollSteps, bpm = 120) {
    const ticksPer16th = this.ppqn / 4; // 120 ticks per 16th note step
    const events = [];

    // 1. Melodic Piano Roll Notes (Channel 0 / MIDI Channel 1)
    if (pianoRollSteps && Array.isArray(pianoRollSteps)) {
      pianoRollSteps.forEach((step, sIdx) => {
        if (step && step.note) {
          const startTick = sIdx * ticksPer16th;
          const gate = step.gate || 0.8;
          const duration = Math.max(10, Math.round(ticksPer16th * gate));
          const endTick = startTick + duration;
          const vel = Math.round(Math.max(1, Math.min(127, (step.vel || 0.85) * 127)));

          // Note On event
          events.push({
            tick: startTick,
            type: 0x90, // Note On Ch 1
            data1: step.note & 0x7F,
            data2: vel
          });

          // Note Off event
          events.push({
            tick: endTick,
            type: 0x80, // Note Off Ch 1
            data1: step.note & 0x7F,
            data2: 0
          });
        }
      });
    }

    // 2. Drum Track Hits (Channel 9 / MIDI Channel 10)
    if (drumTracks && Array.isArray(drumTracks)) {
      drumTracks.forEach(track => {
        if (track.pattern && Array.isArray(track.pattern)) {
          track.pattern.forEach((active, sIdx) => {
            if (active === 1) {
              const startTick = sIdx * ticksPer16th;
              const duration = 60; // 60 ticks drum gate
              const endTick = startTick + duration;

              events.push({
                tick: startTick,
                type: 0x99, // Note On Ch 10
                data1: track.note & 0x7F,
                data2: 100 // Drum velocity
              });

              events.push({
                tick: endTick,
                type: 0x89, // Note Off Ch 10
                data1: track.note & 0x7F,
                data2: 0
              });
            }
          });
        }
      });
    }

    // Sort events chronologically by absolute tick
    events.sort((a, b) => {
      if (a.tick === b.tick) {
        // Put Note Offs before Note Ons at the same tick
        return (a.type & 0xF0) === 0x80 ? -1 : 1;
      }
      return a.tick - b.tick;
    });

    // 3. Build Track Chunk Payload with Delta Times
    const trackBytes = [];

    // Meta Event: Set Tempo (Microseconds per Quarter Note)
    // usPerBeat = 60,000,000 / BPM
    const usPerBeat = Math.round(60000000 / bpm);
    trackBytes.push(0x00); // delta = 0
    trackBytes.push(0xFF, 0x51, 0x03);
    trackBytes.push((usPerBeat >> 16) & 0xFF, (usPerBeat >> 8) & 0xFF, usPerBeat & 0xFF);

    // Meta Event: Time Signature (4/4, 24 MIDI clocks per metronome click, 8 32nd notes per 24 clocks)
    trackBytes.push(0x00); // delta = 0
    trackBytes.push(0xFF, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08);

    // Meta Event: Track Name
    const trackName = "AMY Studio Pattern";
    const nameBytes = new TextEncoder().encode(trackName);
    trackBytes.push(0x00); // delta = 0
    trackBytes.push(0xFF, 0x03, nameBytes.length, ...nameBytes);

    let lastTick = 0;
    events.forEach(ev => {
      const delta = ev.tick - lastTick;
      lastTick = ev.tick;

      const deltaBytes = this.encodeVLQ(delta);
      trackBytes.push(...deltaBytes);
      trackBytes.push(ev.type, ev.data1, ev.data2);
    });

    // End of Track Meta Event
    trackBytes.push(0x00); // delta = 0
    trackBytes.push(0xFF, 0x2F, 0x00);

    // 4. Assemble Final File Buffer
    // Header Chunk: 'MThd' + length (6) + format (0) + tracks (1) + ppqn (480)
    const headerBytes = [
      0x4D, 0x54, 0x68, 0x64, // 'MThd'
      0x00, 0x00, 0x00, 0x06, // length 6
      0x00, 0x00,             // Format 0
      0x00, 0x01,             // 1 Track
      (this.ppqn >> 8) & 0xFF, this.ppqn & 0xFF
    ];

    // Track Chunk Header: 'MTrk' + length (4 bytes)
    const trackHeader = [
      0x4D, 0x54, 0x72, 0x6B, // 'MTrk'
      (trackBytes.length >> 24) & 0xFF,
      (trackBytes.length >> 16) & 0xFF,
      (trackBytes.length >> 8) & 0xFF,
      trackBytes.length & 0xFF
    ];

    const fileBuffer = new Uint8Array(headerBytes.length + trackHeader.length + trackBytes.length);
    fileBuffer.set(headerBytes, 0);
    fileBuffer.set(trackHeader, headerBytes.length);
    fileBuffer.set(trackBytes, headerBytes.length + trackHeader.length);

    return fileBuffer;
  }

  downloadMidi(drumTracks, pianoRollSteps, bpm = 120, filename = "amy_pattern.mid") {
    const bytes = this.exportMidiFile(drumTracks, pianoRollSteps, bpm);
    const blob = new Blob([bytes], { type: 'audio/midi' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    a.click();
    URL.revokeObjectURL(url);
    console.log(`[MIDI File Handler] Exported ${filename} (${bytes.length} bytes).`);
  }

  // ══════════════════════════════════════════════════════════
  // MIDI FILE IMPORT (SMF PARSER)
  // ══════════════════════════════════════════════════════════
  importMidiFile(arrayBuffer) {
    const bytes = new Uint8Array(arrayBuffer);
    if (bytes[0] !== 0x4D || bytes[1] !== 0x54 || bytes[2] !== 0x68 || bytes[3] !== 0x64) {
      throw new Error("Arquivo não é um Standard MIDI File válido (cabeçalho MThd ausente).");
    }

    const division = (bytes[12] << 8) | bytes[13];
    const ticksPerQuarter = division & 0x7FFF;
    const ticksPer16th = ticksPerQuarter / 4;

    let offset = 14;
    let detectedBpm = 120;
    const importedSteps = new Array(16).fill(null);
    const importedDrums = [
      { note: 36, pattern: new Array(16).fill(0) }, // KICK
      { note: 38, pattern: new Array(16).fill(0) }, // SNARE
      { note: 42, pattern: new Array(16).fill(0) }, // CH
      { note: 46, pattern: new Array(16).fill(0) }, // OH
      { note: 39, pattern: new Array(16).fill(0) }, // CLAP
      { note: 45, pattern: new Array(16).fill(0) }, // TOM
      { note: 37, pattern: new Array(16).fill(0) }, // RIM
      { note: 49, pattern: new Array(16).fill(0) }  // CRASH
    ];

    while (offset < bytes.length) {
      if (bytes[offset] === 0x4D && bytes[offset + 1] === 0x54 && bytes[offset + 2] === 0x72 && bytes[offset + 3] === 0x6B) {
        // Track Chunk 'MTrk'
        const chunkLen = (bytes[offset + 4] << 24) | (bytes[offset + 5] << 16) | (bytes[offset + 6] << 8) | bytes[offset + 7];
        const trackStart = offset + 8;
        const trackEnd = trackStart + chunkLen;

        let ptr = trackStart;
        let currentTick = 0;
        let runningStatus = 0;

        while (ptr < trackEnd && ptr < bytes.length) {
          const delta = this.decodeVLQ(bytes, ptr);
          ptr += delta.bytesRead;
          currentTick += delta.value;

          let statusByte = bytes[ptr];
          if (statusByte < 0x80) {
            statusByte = runningStatus; // Running status
          } else {
            ptr++;
            runningStatus = statusByte;
          }

          if (statusByte === 0xFF) {
            // Meta event
            const metaType = bytes[ptr++];
            const metaLen = this.decodeVLQ(bytes, ptr);
            ptr += metaLen.bytesRead;

            if (metaType === 0x51 && metaLen.value === 3) {
              // Set Tempo
              const usPerQuarter = (bytes[ptr] << 16) | (bytes[ptr + 1] << 8) | bytes[ptr + 2];
              detectedBpm = Math.round(60000000 / usPerQuarter);
            }
            ptr += metaLen.value;
          } else if (statusByte === 0xF0 || statusByte === 0xF7) {
            // SysEx event
            const syxLen = this.decodeVLQ(bytes, ptr);
            ptr += syxLen.bytesRead + syxLen.value;
          } else {
            // Channel Voice Message
            const msgType = statusByte & 0xF0;
            const channel = statusByte & 0x0F;
            const data1 = bytes[ptr++];
            const data2 = (msgType === 0xC0 || msgType === 0xD0) ? 0 : bytes[ptr++];

            // Check Note On (velocity > 0)
            if (msgType === 0x90 && data2 > 0) {
              const stepIdx = Math.round(currentTick / ticksPer16th) % 16;

              if (channel === 9) {
                // Drum Track hit
                const dTrack = importedDrums.find(d => d.note === data1);
                if (dTrack) {
                  dTrack.pattern[stepIdx] = 1;
                }
              } else {
                // Melodic Note
                if (!importedSteps[stepIdx]) {
                  importedSteps[stepIdx] = {
                    note: data1,
                    vel: parseFloat((data2 / 127.0).toFixed(2)),
                    gate: 0.8,
                    ratchet: 1,
                    prob: 1.0
                  };
                }
              }
            }
          }
        }
        offset = trackEnd;
      } else {
        offset++;
      }
    }

    return {
      bpm: detectedBpm,
      pianoRollSteps: importedSteps,
      drumTracks: importedDrums
    };
  }
}

if (typeof window !== 'undefined') {
  window.standardMidiFileHandler = new StandardMidiFileHandler();
}
if (typeof module !== 'undefined' && module.exports) {
  module.exports = { StandardMidiFileHandler };
}
