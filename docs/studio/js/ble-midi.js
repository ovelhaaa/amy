/**
 * AMY Studio - Web Bluetooth MIDI (BLE MIDI) Manager
 * Connects wirelessly to M-VAVE SMK25 V2, ESP32-S3 and other BLE MIDI controllers
 * using the standard MIDI over Bluetooth Low Energy specification (UUID: 03b80e5a...).
 */

const BLE_MIDI_SERVICE_UUID = '03b80e5a-ede8-4b33-a028-0aec04ca965f';
const BLE_MIDI_CHAR_UUID    = '7772e5db-3868-4112-a1a9-f2669d106bf3';

class BleMidiManager {
  constructor() {
    this.device = null;
    this.server = null;
    this.characteristic = null;
    this.isConnected = false;
    this.onStateChange = null;
  }

  isSupported() {
    return typeof navigator !== 'undefined' && !!navigator.bluetooth;
  }

  async connect() {
    if (!this.isSupported()) {
      alert("Web Bluetooth não é suportado neste navegador. Utilize o Google Chrome, Edge ou Opera em HTTPS ou localhost.");
      return false;
    }

    try {
      console.log("[BLE MIDI] Procurando dispositivos Bluetooth MIDI...");
      
      this.device = await navigator.bluetooth.requestDevice({
        filters: [
          { services: [BLE_MIDI_SERVICE_UUID] },
          { namePrefix: "SMK" },
          { namePrefix: "M-VAVE" },
          { namePrefix: "AMY" },
          { namePrefix: "ESP32" }
        ],
        optionalServices: [BLE_MIDI_SERVICE_UUID]
      });

      this.device.addEventListener('gattserverdisconnected', () => this.handleDisconnect());

      console.log(`[BLE MIDI] Conectando ao dispositivo: ${this.device.name || 'Dispositivo Desconhecido'}...`);
      this.server = await this.device.gatt.connect();

      const service = await this.server.getPrimaryService(BLE_MIDI_SERVICE_UUID);
      this.characteristic = await service.getCharacteristic(BLE_MIDI_CHAR_UUID);

      await this.characteristic.startNotifications();
      this.characteristic.addEventListener('characteristicvaluechanged', (e) => this.handlePacket(e));

      this.isConnected = true;
      console.log("[BLE MIDI] Conectado e escutando eventos com sucesso!");

      if (this.onStateChange) this.onStateChange(true, this.device.name);
      return true;
    } catch (err) {
      console.warn("[BLE MIDI] Falha ou cancelamento na conexão Bluetooth:", err);
      this.handleDisconnect();
      return false;
    }
  }

  async disconnect() {
    if (this.device && this.device.gatt.connected) {
      await this.device.gatt.disconnect();
    }
    this.handleDisconnect();
  }

  async toggleConnect() {
    if (this.isConnected) {
      await this.disconnect();
    } else {
      await this.connect();
    }
  }

  handleDisconnect() {
    this.isConnected = false;
    this.device = null;
    this.server = null;
    this.characteristic = null;
    console.log("[BLE MIDI] Dispositivo desconectado.");
    if (this.onStateChange) this.onStateChange(false, null);
  }

  // ══════════════════════════════════════════════════════════
  // BLE PACKET PARSER
  // ══════════════════════════════════════════════════════════
  handlePacket(event) {
    const data = new Uint8Array(event.target.value.buffer);
    if (data.length < 3) return;

    // Standard BLE MIDI Packet Structure:
    // Byte 0: Header (10xxxxxx)
    // Byte 1: Timestamp-low (1xxxxxxx)
    // Followed by MIDI status and data bytes
    let idx = 1;
    let runningStatus = 0;

    while (idx < data.length) {
      // Check for intermediate timestamp bytes (bit 7 = 1, but not a MIDI status byte)
      // Standard MIDI status is 0x80..0xFF
      const b = data[idx];

      if (b >= 0x80) {
        // Could be a Timestamp byte or a MIDI Status byte
        if (idx + 1 < data.length && (data[idx + 1] & 0x80) === 0x80) {
          // It's a timestamp byte preceding a status byte
          idx++;
          continue;
        }

        // It's a Status byte
        runningStatus = b;
        idx++;

        const msgType = runningStatus & 0xF0;
        const channel = runningStatus & 0x0F;

        if (msgType === 0x90 || msgType === 0x80 || msgType === 0xB0 || msgType === 0xE0) {
          if (idx + 1 < data.length) {
            const data1 = data[idx++];
            const data2 = data[idx++];
            this.dispatchMidi(runningStatus, channel, data1, data2);
          }
        } else if (msgType === 0xC0 || msgType === 0xD0) {
          if (idx < data.length) {
            const data1 = data[idx++];
            this.dispatchMidi(runningStatus, channel, data1, 0);
          }
        }
      } else {
        // Running status message
        if (runningStatus !== 0) {
          const msgType = runningStatus & 0xF0;
          const channel = runningStatus & 0x0F;

          if (msgType === 0x90 || msgType === 0x80 || msgType === 0xB0 || msgType === 0xE0) {
            const data1 = b;
            idx++;
            if (idx < data.length) {
              const data2 = data[idx++];
              this.dispatchMidi(runningStatus, channel, data1, data2);
            }
          }
        } else {
          idx++;
        }
      }
    }
  }

  dispatchMidi(status, channel, data1, data2) {
    const msgType = status & 0xF0;

    // Log to monitor
    if (window.amyAudioBridge && window.amyAudioBridge.onMidiLog) {
      window.amyAudioBridge.onMidiLog({
        type: 'MIDI',
        source: 'BLE',
        data: [status, data1, data2]
      });
    }

    // 1. Note On / Note Off
    if (msgType === 0x90) {
      if (data2 > 0) {
        const velNorm = data2 / 127.0;
        // Trigger Keyboard / Engine
        if (window.studioKeyboard) {
          window.studioKeyboard.triggerNoteOn(data1);
        } else if (window.amyAudioBridge) {
          window.amyAudioBridge.noteOn(channel, data1, velNorm);
        }
      } else {
        // Velocity 0 = Note Off
        if (window.studioKeyboard) {
          window.studioKeyboard.triggerNoteOff(data1);
        } else if (window.amyAudioBridge) {
          window.amyAudioBridge.noteOff(channel, data1);
        }
      }
    } else if (msgType === 0x80) {
      if (window.studioKeyboard) {
        window.studioKeyboard.triggerNoteOff(data1);
      } else if (window.amyAudioBridge) {
        window.amyAudioBridge.noteOff(channel, data1);
      }
    } else if (msgType === 0xB0) {
      // 2. Control Change
      if (window.midiLearnManager) {
        window.midiLearnManager.processMidiMessage(channel, data1, data2);
      }
      if (window.amyAudioBridge) {
        window.amyAudioBridge.controlChange(channel, data1, data2);
      }
    } else if (msgType === 0xE0) {
      // 3. Pitch Bend (14-bit: data1 = LSB, data2 = MSB)
      const bend14 = ((data2 << 7) | data1) - 8192;
      const normBend = bend14 / 8192.0;
      if (window.studioKeyboard) {
        window.studioKeyboard.setPitchBend(normBend);
      }
    }
  }

  async sendMidi(bytes) {
    if (!this.isConnected || !this.characteristic) return;
    try {
      // Package as BLE MIDI: [Header 0x80, Timestamp 0x80, ...bytes]
      const packet = new Uint8Array([0x80, 0x80, ...bytes]);
      await this.characteristic.writeValueWithoutResponse(packet);
    } catch (err) {
      console.warn("[BLE MIDI] Erro ao enviar pacote MIDI:", err);
    }
  }
}

if (typeof window !== 'undefined') {
  window.bleMidiManager = new BleMidiManager();
}
if (typeof module !== 'undefined' && module.exports) {
  module.exports = { BleMidiManager };
}
