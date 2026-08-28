/**
 * AMY Studio - Yamaha DX7 SysEx (.syx) Voice Bank Importer & Converter
 * Decodes 32-voice DX7 cartridge bank dumps and 1-voice SysEx files into canonical AMY patches.
 */

class Dx7SysExImporter {
  constructor() {}

  /**
   * Parse a raw Uint8Array from a .syx file
   * @param {Uint8Array} bytes
   * @returns {Array<Object>} List of decoded patches
   */
  static parseSysEx(bytes) {
    const patches = [];

    // Case 1: 4096-byte or 4104-byte 32-Voice Bank Dump
    // Header typically starts with 0xF0, 0x43, [substatus/channel], 0x09, 0x20, 0x00 ...
    if (bytes.length >= 4096) {
      let dataOffset = 0;
      // Find payload start after SysEx header
      if (bytes[0] === 0xF0) {
        dataOffset = 6; // Standard Yamaha 32-voice header length
      }

      for (let i = 0; i < 32; i++) {
        const voicePacked = bytes.slice(dataOffset + i * 128, dataOffset + (i + 1) * 128);
        if (voicePacked.length >= 128) {
          const unpacked = Dx7SysExImporter.unpackVoice(voicePacked);
          const patch = Dx7SysExImporter.decodeUnpackedVoice(unpacked, i + 1);
          patches.push(patch);
        }
      }
    } 
    // Case 2: 156-byte or 163-byte Single Voice Dump
    else if (bytes.length >= 128) {
      let dataOffset = 0;
      if (bytes[0] === 0xF0) dataOffset = 6;
      const voiceData = bytes.slice(dataOffset, dataOffset + 156);
      const patch = Dx7SysExImporter.decodeUnpackedVoice(voiceData, 1);
      patches.push(patch);
    }

    return patches;
  }

  static unpackVoice(packed) {
    // Unpack 128 packed bytes into 156 unpacked DX7 parameter bytes
    const unpacked = new Uint8Array(156);
    let u = 0;

    for (let op = 0; op < 6; op++) {
      const pOff = op * 17;
      unpacked[u++] = packed[pOff + 0]; // R1
      unpacked[u++] = packed[pOff + 1]; // R2
      unpacked[u++] = packed[pOff + 2]; // R3
      unpacked[u++] = packed[pOff + 3]; // R4
      unpacked[u++] = packed[pOff + 4]; // L1
      unpacked[u++] = packed[pOff + 5]; // L2
      unpacked[u++] = packed[pOff + 6]; // L3
      unpacked[u++] = packed[pOff + 7]; // L4
      unpacked[u++] = packed[pOff + 8]; // Breakpoint
      unpacked[u++] = packed[pOff + 9]; // Left Depth
      unpacked[u++] = packed[pOff + 10]; // Right Depth
      
      const b11 = packed[pOff + 11];
      unpacked[u++] = b11 & 0x03; // Left Curve
      unpacked[u++] = (b11 >> 2) & 0x03; // Right Curve
      
      const b12 = packed[pOff + 12];
      unpacked[u++] = b12 & 0x07; // Rate Scaling
      
      const b13 = packed[pOff + 13];
      unpacked[u++] = b13 & 0x03; // Amp Mod Sens
      unpacked[u++] = (b13 >> 2) & 0x07; // Key Vel Sens
      
      unpacked[u++] = packed[pOff + 14]; // Total Level
      
      const b15 = packed[pOff + 15];
      unpacked[u++] = b15 & 0x01; // Osc Mode (0=Ratio, 1=Fixed)
      unpacked[u++] = (b15 >> 1) & 0x1F; // Coarse
      
      const b16 = packed[pOff + 16];
      unpacked[u++] = b16 & 0x0F; // Fine
      unpacked[u++] = (b16 >> 4) & 0x0F; // Detune (-7..+7)
    }

    // Unpack pitch envelope & global parameters (offset 102..127)
    for (let i = 102; i < 128; i++) {
      if (u < 156) unpacked[u++] = packed[i];
    }

    return unpacked;
  }

  static decodeUnpackedVoice(unpacked, index = 1) {
    // Extract Voice Name (last 10 bytes)
    let nameChars = "";
    for (let n = 145; n < 155; n++) {
      const c = unpacked[n];
      nameChars += (c >= 32 && c <= 126) ? String.fromCharCode(c) : " ";
    }
    const name = nameChars.trim() || `DX7 Patch ${index}`;
    const algo = (unpacked[134] & 0x1F) + 1; // 1..32
    const feedback = unpacked[135] & 0x07; // 0..7

    // Extract Operators 6 down to 1
    const operators = [];
    for (let op = 0; op < 6; op++) {
      const off = op * 21;
      const r1 = unpacked[off + 0];
      const r2 = unpacked[off + 1];
      const r3 = unpacked[off + 2];
      const r4 = unpacked[off + 3];
      const l1 = unpacked[off + 4];
      const l2 = unpacked[off + 5];
      const l3 = unpacked[off + 6];
      const l4 = unpacked[off + 7];
      const outLevel = unpacked[off + 16];
      const mode = unpacked[off + 17];
      const coarse = unpacked[off + 18];
      const fine = unpacked[off + 19];
      const detune = unpacked[off + 20] - 7;

      let ratio = (coarse === 0 ? 0.5 : coarse) * (1.0 + (fine / 100.0));

      operators.push({
        opNum: 6 - op,
        ratio: ratio,
        level: outLevel / 99.0,
        detune: detune,
        rates: [r1, r2, r3, r4],
        levels: [l1, l2, l3, l4]
      });
    }

    return {
      id: 1024 + index,
      name: name,
      category: "DX7 Imported",
      type: "DX7",
      algorithm: algo,
      feedback: feedback / 7.0,
      operators: operators,
      wireCommand: `v0w8o${algo}b${(feedback / 7.0).toFixed(3)}Z`
    };
  }
}

window.Dx7SysExImporter = Dx7SysExImporter;
