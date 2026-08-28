/**
 * AMY Studio - ESP32-S3 Hardware Sync, Serial Bridge & Flash SPIFFS Manager
 * Handles bidirectional WebSerial communication, SPIFFS patch backup/upload,
 * and live system telemetry from the ESP32-S3 DevKitC.
 */

class ESP32HardwareSyncManager {
  constructor() {
    this.serialPort = null;
    this.serialReader = null;
    this.serialWriter = null;
    this.isConnected = false;
    this.baudRate = 115200;
    this.liveSyncEnabled = true;
    this.flashPatches = []; // List of patches on ESP32 SPIFFS
    this.telemetry = {
      cpuFreqMhz: 240,
      dspLoadPercent: 0,
      activeVoices: 0,
      underruns: 0,
      freeRamKb: 320,
      freePsramMb: 8.0,
      spiffsUsedKb: 0,
      spiffsTotalKb: 8192,
      connectedUsbDevice: "M-VAVE SMK25 V2"
    };

    this.onStatusChange = null;
    this.onLogMessage = null;
    this.onTelemetryUpdate = null;
    this.onPatchListUpdate = null;
  }

  // ══════════════════════════════════════════════════════════
  // SERIAL CONNECTION (WebSerial)
  // ══════════════════════════════════════════════════════════
  async connectSerial() {
    if (!('serial' in navigator)) {
      alert("WebSerial API não é suportada neste navegador. Use o Google Chrome ou Microsoft Edge.");
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
      this.log(`[ESP32 Serial] Conectado com sucesso na porta serial (${this.baudRate} baud).`, "success");
      
      if (this.onStatusChange) this.onStatusChange({ connected: true, type: 'SERIAL' });

      // Request initial status and patch list from ESP32
      setTimeout(() => {
        this.sendSerialCommand("status\n");
        this.sendSerialCommand("patch list\n");
        this.sendSerialCommand("audio status\n");
      }, 300);

      return true;
    } catch (err) {
      console.error("[ESP32 Serial] Erro ao conectar:", err);
      this.log(`[ESP32 Serial] Falha na conexão: ${err.message}`, "error");
      return false;
    }
  }

  async disconnectSerial() {
    try {
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
    } catch (e) {
      console.log("[ESP32 Serial] Cleanup:", e);
    }

    this.isConnected = false;
    this.log("[ESP32 Serial] Desconectado.", "warn");
    if (this.onStatusChange) this.onStatusChange({ connected: false });
  }

  async readSerialLoop() {
    const textDecoder = new TextDecoderStream();
    this.serialPort.readable.pipeTo(textDecoder.writable);
    this.serialReader = textDecoder.readable.getReader();

    let buffer = "";

    try {
      while (true) {
        const { value, done } = await this.serialReader.read();
        if (done) break;
        if (value) {
          buffer += value;
          const lines = buffer.split('\n');
          buffer = lines.pop(); // keep unfinished remainder

          for (const line of lines) {
            const cleanLine = line.trim();
            if (cleanLine.length > 0) {
              this.handleIncomingLine(cleanLine);
            }
          }
        }
      }
    } catch (err) {
      console.warn("[ESP32 Serial] Stream finalizado:", err);
    }
  }

  handleIncomingLine(line) {
    this.log(line, "info");

    // Parse status responses
    if (line.includes("DSP LOAD:") || line.includes("DSP Load:")) {
      const match = line.match(/(\d+(\.\d+)?)%/);
      if (match) this.telemetry.dspLoadPercent = parseFloat(match[1]);
      if (this.onTelemetryUpdate) this.onTelemetryUpdate(this.telemetry);
    }

    if (line.includes("UNDERRUNS:") || line.includes("Underruns:")) {
      const match = line.match(/UNDERRUNS:\s*(\d+)/i);
      if (match) this.telemetry.underruns = parseInt(match[1]);
      if (this.onTelemetryUpdate) this.onTelemetryUpdate(this.telemetry);
    }

    if (line.startsWith("[#") && line.includes("]")) {
      // Patch list line format: [#000] Patch Name - Category
      const match = line.match(/\[#(\d+)\]\s*([^-]+)\s*-\s*(.+)/);
      if (match) {
        const slot = parseInt(match[1]);
        const name = match[2].trim();
        const cat = match[3].trim();
        this.flashPatches.push({ slot, name, cat });
        if (this.onPatchListUpdate) this.onPatchListUpdate(this.flashPatches);
      }
    }
  }

  async sendSerialCommand(cmdStr) {
    if (!this.isConnected || !this.serialWriter) return;
    try {
      const payload = cmdStr.endsWith('\n') ? cmdStr : cmdStr + '\n';
      await this.serialWriter.write(payload);
    } catch (err) {
      console.error("[ESP32 Serial] Erro ao enviar comando:", err);
    }
  }

  sendWireIfConnected(wireMsg) {
    if (!this.isConnected || !this.liveSyncEnabled) return;
    this.sendSerialCommand(wireMsg);
  }

  noteOn(channel, note, velocity) {
    if (!this.isConnected) return;
    this.sendSerialCommand(`v0n${note}l${velocity.toFixed(3)}Z`);
  }

  noteOff(channel, note) {
    if (!this.isConnected) return;
    this.sendSerialCommand(`v0n${note}l0Z`);
  }

  pitchBend(channel, bendVal) {
    if (!this.isConnected) return;
    this.sendSerialCommand(`v0f,,,,,,${bendVal.toFixed(3)}Z`);
  }

  controlChange(channel, cc, val) {
    if (!this.isConnected) return;
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

    this.log(`[Flash Manager] Gravando Patch #${slotId} ("${patchData.name}") na Flash SPIFFS...`, "info");
    
    // 1. Send synth wire command to set parameters on active voice
    if (window.synthStateManager) {
      window.synthStateManager.applyFullPatch();
    }

    // 2. Instruct firmware to persist active patch into Flash slotId
    this.sendSerialCommand(`patch save ${slotId}`);
    this.log(`[Flash Manager] Patch #${slotId} gravado com sucesso!`, "success");
    
    // Refresh patch list
    setTimeout(() => this.sendSerialCommand("patch list"), 500);
    return true;
  }

  async loadPatchFromFlash(slotId) {
    if (!this.isConnected) return;
    this.log(`[Flash Manager] Carregando Patch #${slotId} da Flash SPIFFS...`, "info");
    this.sendSerialCommand(`patch load ${slotId}`);
  }

  async refreshFlashPatchList() {
    if (!this.isConnected) return;
    this.flashPatches = [];
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
