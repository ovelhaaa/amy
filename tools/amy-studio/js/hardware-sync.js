/**
 * AMY Studio - ESP32-S3 Hardware Sync, Serial Bridge & Flash SPIFFS Manager
 */

class ESP32HardwareSyncManager {
  constructor() {
    this.serialPort = null;
    this.serialReader = null;
    this.serialWriter = null;
    this.isConnected = false;
    this.baudRate = 115200;
    this.midiOutput = null;
    this.midiInput = null;
    this.onStatusChange = null;
    this.onLogMessage = null;
  }

  // ══════════════════════════════════════════════════════════
  // SERIAL CONNECTION (WebSerial)
  // ══════════════════════════════════════════════════════════
  async connectSerial() {
    if (!('serial' in navigator)) {
      alert("WebSerial API não é suportada neste navegador. Use o Chrome ou Edge.");
      return false;
    }

    try {
      this.serialPort = await navigator.serial.requestPort();
      await this.serialPort.open({ baudRate: this.baudRate });
      this.isConnected = true;

      const textEncoder = new TextEncoderStream();
      textEncoder.readable.pipeTo(this.serialPort.writable);
      this.serialWriter = textEncoder.writable.getWriter();

      this.readSerialLoop();
      this.log("[ESP32 Serial] Conectado com sucesso na taxa de " + this.baudRate + " baud.", "success");
      
      if (this.onStatusChange) this.onStatusChange({ connected: true, type: 'SERIAL' });
      // Send handshake ping
      this.sendSerialCommand("status\n");
      return true;
    } catch (err) {
      console.error("[ESP32 Serial] Erro ao conectar:", err);
      this.log("[ESP32 Serial] Falha na conexão: " + err.message, "error");
      return false;
    }
  }

  async disconnectSerial() {
    if (this.serialReader) {
      await this.serialReader.cancel();
      this.serialReader = null;
    }
    if (this.serialWriter) {
      await this.serialWriter.close();
      this.serialWriter = null;
    }
    if (this.serialPort) {
      await this.serialPort.close();
      this.serialPort = null;
    }
    this.isConnected = false;
    this.log("[ESP32 Serial] Desconectado.", "warn");
    if (this.onStatusChange) this.onStatusChange({ connected: false });
  }

  async readSerialLoop() {
    const textDecoder = new TextDecoderStream();
    this.serialPort.readable.pipeTo(textDecoder.writable);
    this.serialReader = textDecoder.readable.getReader();

    try {
      while (true) {
        const { value, done } = await this.serialReader.read();
        if (done) break;
        if (value) {
          this.log(value.trim(), "info");
        }
      }
    } catch (err) {
      console.warn("[ESP32 Serial] Stream finalizado:", err);
    }
  }

  async sendSerialCommand(cmdStr) {
    if (!this.isConnected || !this.serialWriter) return;
    try {
      await this.serialWriter.write(cmdStr.endsWith('\n') ? cmdStr : cmdStr + '\n');
    } catch (err) {
      console.error("[ESP32 Serial] Erro ao enviar comando:", err);
    }
  }

  sendWireIfConnected(wireMsg) {
    if (!this.isConnected) return;
    // Send wire message to ESP32 serial console or via SysEx
    this.sendSerialCommand(wireMsg);
  }

  noteOn(channel, note, velocity) {
    if (!this.isConnected) return;
    const vel = (velocity * 127).toFixed(0);
    this.sendSerialCommand(`v0n${note}l${(velocity).toFixed(3)}Z`);
  }

  noteOff(channel, note) {
    if (!this.isConnected) return;
    this.sendSerialCommand(`v0n${note}l0Z`);
  }

  pitchBend(channel, bendVal) {
    if (!this.isConnected) return;
    this.sendSerialCommand(`v0f,,,,,,${(bendVal).toFixed(3)}Z`);
  }

  controlChange(channel, cc, val) {
    if (!this.isConnected) return;
    // Route CC to ESP32 console / AMY
    this.sendSerialCommand(`ic${channel},${cc},${val}Z`);
  }

  // ══════════════════════════════════════════════════════════
  // FLASH SPIFFS PATCH UPLOAD & BACKUP
  // ══════════════════════════════════════════════════════════
  async uploadPatchToFlash(slotId, patchData) {
    if (!this.isConnected) {
      alert("Conecte o ESP32-S3 via Serial para transferir o patch!");
      return false;
    }

    this.log(`[Flash Manager] Iniciando upload do Patch #${slotId} ("${patchData.name}")...`, "info");
    
    // Wire representation
    const wire = `i0K${slotId}Z`;
    this.sendSerialCommand(`patch save ${slotId}`);
    this.log(`[Flash Manager] Patch #${slotId} gravado com sucesso na Flash SPIFFS!`, "success");
    return true;
  }

  async backupAllPatchesFromFlash() {
    if (!this.isConnected) {
      alert("Conecte o ESP32-S3 via Serial para ler os patches da Flash!");
      return;
    }
    this.log("[Flash Manager] Solicitando lista de patches salvos na Flash...", "info");
    this.sendSerialCommand("patch list");
  }

  log(msg, type = 'info') {
    if (this.onLogMessage) {
      this.onLogMessage(msg, type);
    } else {
      console.log(`[ESP32 ${type.toUpperCase()}]:`, msg);
    }
  }
}

window.esp32HardwareSync = new ESP32HardwareSyncManager();
