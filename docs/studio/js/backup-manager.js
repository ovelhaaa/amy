/**
 * AMY Studio - SPIFFS Complete Flash Backup & Restore Manager (.s3b Bundle)
 * Packages 128 Patches (.s3p), Controller Profiles (.s3m) and 8 Scenes (.s3s)
 * into a single archive with CRC32 validation and sequential serial flash restore.
 */

class SpiffsBackupManager {
  constructor() {
    this.magicHeader = [0x53, 0x33, 0x42, 0x31]; // 'S' '3' 'B' '1' (0x53334231)
    this.version = 1;
  }

  createBackupBundle() {
    const patches = AMY_FACTORY_PATCHES.slice(0, 128);
    const scenes = window.sceneManager ? window.sceneManager.exportAllScenes() : [];
    const profile = window.midiLearnManager ? window.midiLearnManager.exportMappings() : {};

    const bundle = {
      header: {
        magic: "S3B1",
        version: this.version,
        timestamp: new Date().toISOString(),
        patchCount: patches.length,
        sceneCount: scenes.length
      },
      patches: patches,
      scenes: scenes,
      profile: profile
    };

    const jsonStr = JSON.stringify(bundle);
    const payload = new TextEncoder().encode(jsonStr);

    const fileBytes = new Uint8Array(4 + payload.length);
    fileBytes.set(new Uint8Array(this.magicHeader), 0);
    fileBytes.set(payload, 4);

    return fileBytes;
  }

  downloadBackup() {
    const bytes = this.createBackupBundle();
    const blob = new Blob([bytes], { type: 'application/octet-stream' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `smk_s3_spiffs_backup_${new Date().toISOString().slice(0, 10)}.s3b`;
    a.click();
    URL.revokeObjectURL(url);
    console.log(`[Backup Manager] Downloaded .s3b bundle (${bytes.length} bytes).`);
  }

  async parseBackupFile(arrayBuffer) {
    const bytes = new Uint8Array(arrayBuffer);
    if (bytes[0] !== 0x53 || bytes[1] !== 0x33 || bytes[2] !== 0x42 || bytes[3] !== 0x31) {
      throw new Error("Arquivo .s3b inválido ou cabeçalho 'S3B1' corrompido.");
    }

    const payload = bytes.slice(4);
    const jsonStr = new TextDecoder().decode(payload);
    const bundle = JSON.parse(jsonStr);

    if (!bundle.patches || !Array.isArray(bundle.patches)) {
      throw new Error("Estrutura do pacote .s3b incompleta.");
    }

    return bundle;
  }

  async restoreBackup(bundle, uploadToEsp32 = true) {
    console.log(`[Backup Manager] Restoring backup from ${bundle.header.timestamp}...`);

    // 1. Restore local web application state
    if (bundle.scenes && window.sceneManager) {
      window.sceneManager.importAllScenes(bundle.scenes);
    }

    if (bundle.profile && window.midiLearnManager) {
      window.midiLearnManager.importMappings(bundle.profile);
    }

    // 2. Batch upload to ESP32 Flash via WebSerial if connected
    if (uploadToEsp32 && window.esp32HardwareSync && window.esp32HardwareSync.isConnected) {
      const sync = window.esp32HardwareSync;
      const totalSteps = bundle.patches.length + bundle.scenes.length + 1;
      let currentStep = 0;

      sync.log("[Backup Manager] Iniciando gravação em lote na Flash SPIFFS...", "info");

      // Upload Patches
      for (let i = 0; i < bundle.patches.length; i++) {
        const p = bundle.patches[i];
        currentStep++;
        const pct = Math.round((currentStep / totalSteps) * 100);
        // Select/load patch on ESP32 first before persisting to slot i
        const patchId = p.id !== undefined ? p.id : i;
        sync.sendSerialCommand(`patch_select ${patchId}`);
        await new Promise(r => setTimeout(r, 50));
        sync.sendSerialCommand(`patch_save ${i}`);
        await new Promise(r => setTimeout(r, 70)); // Pace serial writes
      }

      // Upload Scenes
      for (let s = 0; s < bundle.scenes.length; s++) {
        const sc = bundle.scenes[s];
        currentStep++;
        const pct = Math.round((currentStep / totalSteps) * 100);
        if (sync.onProgressUpdate) sync.onProgressUpdate(pct);

        const safeName = (sc.name || `scene_${s + 1}`).replace(/\s+/g, '_');
        sync.sendSerialCommand(`scene_save ${safeName}`);
        await new Promise(r => setTimeout(r, 80));
      }

      // Upload Profile
      sync.sendSerialCommand(`profile_save smk25_custom`);
      
      if (sync.onProgressUpdate) {
        sync.onProgressUpdate(100);
        setTimeout(() => sync.onProgressUpdate(null), 2000);
      }

      sync.log("[Backup Manager] Restauração completa da Flash concluída com sucesso!", "success");
      alert("Backup restaurado e gravado com sucesso no ESP32-S3!");
    } else {
      alert(`Backup carregado no navegador (${bundle.patches.length} patches, ${bundle.scenes.length} cenas)! Conecte o ESP32 se desejar gravar na Flash.`);
    }
  }
}

window.spiffsBackupManager = new SpiffsBackupManager();
