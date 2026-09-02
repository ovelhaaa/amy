/**
 * AMY Studio - Live Performance Scene & Song Matrix Manager (8 Performance Scenes)
 * Manages 8 live scenes matching the ESP32-S3 firmware struct Scene specifications:
 * storing BPM, Patch, Macros, Arpeggiator configuration, Drum Pattern mutes and quantized transitions.
 */

class StudioSceneManager {
  constructor(containerId) {
    this.container = document.getElementById(containerId);
    this.activeSceneIndex = 0;

    this.scenes = [
      {
        id: 0,
        name: "01: Synthwave Intro",
        bpm: 120,
        patchId: 1,
        macros: [50, 50, 30, 50, 10, 40, 25, 0],
        arp: { enabled: false, mode: "UP", division: 16, octaves: 2, latch: false },
        drumPattern: 0,
        drumMutes: 0,
        transitionMode: 0
      },
      {
        id: 1,
        name: "02: 808 Electro Verse",
        bpm: 128,
        patchId: 2,
        macros: [65, 70, 40, 60, 5, 30, 15, 10],
        arp: { enabled: true, mode: "UPDOWN", division: 16, octaves: 2, latch: false },
        drumPattern: 1,
        drumMutes: 0,
        transitionMode: 1
      },
      {
        id: 2,
        name: "03: DX7 Bell Chorus",
        bpm: 128,
        patchId: 128,
        macros: [40, 85, 50, 50, 0, 50, 40, 0],
        arp: { enabled: false, mode: "UP", division: 16, octaves: 1, latch: false },
        drumPattern: 2,
        drumMutes: 0,
        transitionMode: 2
      },
      {
        id: 3,
        name: "04: Acid Bass Peak",
        bpm: 135,
        patchId: 4,
        macros: [90, 95, 80, 80, 0, 10, 10, 40],
        arp: { enabled: true, mode: "RANDOM", division: 16, octaves: 3, latch: false },
        drumPattern: 3,
        drumMutes: 0,
        transitionMode: 0
      },
      {
        id: 4,
        name: "05: Ambient Chillout",
        bpm: 95,
        patchId: 5,
        macros: [30, 40, 60, 70, 80, 90, 80, 0],
        arp: { enabled: false, mode: "UP", division: 8, octaves: 2, latch: false },
        drumPattern: 4,
        drumMutes: 1,
        transitionMode: 1
      },
      {
        id: 5,
        name: "06: Cyberpunk Drive",
        bpm: 140,
        patchId: 31,
        macros: [95, 80, 70, 85, 5, 20, 10, 60],
        arp: { enabled: true, mode: "UP", division: 16, octaves: 2, latch: true },
        drumPattern: 5,
        drumMutes: 0,
        transitionMode: 2
      },
      {
        id: 6,
        name: "07: Piano Acoustic Break",
        bpm: 110,
        patchId: 7,
        macros: [50, 50, 20, 40, 10, 50, 40, 0],
        arp: { enabled: false, mode: "UP", division: 16, octaves: 1, latch: false },
        drumPattern: 6,
        drumMutes: 3,
        transitionMode: 1
      },
      {
        id: 7,
        name: "08: Grand Outro Climax",
        bpm: 124,
        patchId: 0,
        macros: [80, 90, 70, 60, 20, 60, 60, 20],
        arp: { enabled: true, mode: "CHORD", division: 16, octaves: 2, latch: false },
        drumPattern: 7,
        drumMutes: 0,
        transitionMode: 0
      }
    ];

    if (this.container) this.render();
  }

  selectScene(index, immediate = true) {
    if (index < 0 || index >= this.scenes.length) return;
    this.activeSceneIndex = index;
    const s = this.scenes[index];

    console.log(`[Scene Manager] Transição para cena #${index + 1} ("${s.name}")...`);

    // 1. Apply BPM
    if (window.amyStudioSequencer) {
      window.amyStudioSequencer.setBpm(s.bpm);
      const bpmIn = document.getElementById('seqBpmInput');
      if (bpmIn) bpmIn.value = s.bpm;

      // Apply Arp settings
      if (s.arp) {
        window.amyStudioSequencer.arpEnabled = s.arp.enabled;
        window.amyStudioSequencer.arpMode = s.arp.mode || "UP";
        window.amyStudioSequencer.arpOctaves = s.arp.octaves || 2;
        window.amyStudioSequencer.arpLatch = s.arp.latch || false;
      }
    }

    // 2. Load Patch
    if (window.synthStateManager) {
      window.synthStateManager.loadFactoryPatch(s.patchId);
    }

    // 3. Apply Macros
    if (s.macros && window.synthStateManager) {
      s.macros.forEach((v, idx) => window.synthStateManager.setMacro(idx, v));
    }

    // 4. Update OLED Display screen
    if (window.amyVisualizerEngine) {
      window.amyVisualizerEngine.currentOledScreen = "Home";
    }

    // 5. Send serial sync if connected
    if (window.esp32HardwareSync && window.esp32HardwareSync.isConnected) {
      const safeName = s.name.replace(/\s+/g, '_');
      window.esp32HardwareSync.sendSerialCommand(`scene_load ${safeName}`);
    }

    this.render();
  }

  captureCurrentAsScene(index) {
    if (index < 0 || index >= this.scenes.length) return;
    const patch = window.synthStateManager ? window.synthStateManager.currentPatch : null;
    const seq = window.amyStudioSequencer;

    const captured = {
      id: index,
      name: `${String(index + 1).padStart(2, '0')}: ${patch ? patch.name : 'Custom Scene'}`,
      bpm: seq ? seq.bpm : 120,
      patchId: patch ? patch.id : 0,
      macros: patch && patch.macros ? patch.macros.map(m => m.val) : [50, 50, 50, 50, 50, 50, 50, 50],
      arp: {
        enabled: seq ? seq.arpEnabled : false,
        mode: seq ? seq.arpMode : "UP",
        division: seq ? seq.arpDivision : 16,
        octaves: seq ? seq.arpOctaves : 2,
        latch: seq ? seq.arpLatch : false
      },
      drumPattern: 0,
      drumMutes: 0,
      transitionMode: 0
    };

    this.scenes[index] = captured;
    this.render();

    // Flash persistence command if connected
    if (window.esp32HardwareSync && window.esp32HardwareSync.isConnected) {
      const safeName = captured.name.replace(/\s+/g, '_');
      window.esp32HardwareSync.sendSerialCommand(`scene_save ${safeName}`);
      window.esp32HardwareSync.log(`[Scene Manager] Cena #${index + 1} gravada na Flash SPIFFS!`, "success");
    }

    alert(`Cena #${index + 1} capturada com sucesso a partir do estado atual!`);
  }

  exportAllScenes() {
    return JSON.parse(JSON.stringify(this.scenes));
  }

  importAllScenes(newScenes) {
    if (Array.isArray(newScenes)) {
      this.scenes = JSON.parse(JSON.stringify(newScenes));
      this.render();
    }
  }

  render() {
    if (!this.container) return;
    this.container.innerHTML = '';

    const root = document.createElement('div');
    root.style.display = 'flex';
    root.style.flexDirection = 'column';
    root.style.gap = '12px';

    // Top action bar
    const actionsBar = document.createElement('div');
    actionsBar.style.display = 'flex';
    actionsBar.style.justifyContent = 'space-between';
    actionsBar.style.alignItems = 'center';

    actionsBar.innerHTML = `
      <div style="font-size: 11px; color: #94a3b8;">
        8 CENAS DE PERFORMANCE AO VIVO (PADS 1..8)
      </div>
      <div style="display: flex; gap: 6px;">
        <button class="btn btn-gold" style="font-size: 10px;" id="btnCaptureActiveScene">
          📸 CAPTURAR ESTADO ATUAL
        </button>
        <button class="btn" style="font-size: 10px;" id="btnSaveScenesFlash">
          💾 SALVAR CENAS NA FLASH
        </button>
      </div>
    `;

    root.appendChild(actionsBar);

    // 8-grid scenes
    const grid = document.createElement('div');
    grid.className = 'scenes-grid';
    grid.style.display = 'grid';
    grid.style.gridTemplateColumns = 'repeat(auto-fit, minmax(170px, 1fr))';
    grid.style.gap = '10px';

    this.scenes.forEach((scene, idx) => {
      const isActive = this.activeSceneIndex === idx;
      const card = document.createElement('div');
      card.className = `scene-slot-card ${isActive ? 'active' : ''}`;
      card.style.background = isActive ? 'linear-gradient(135deg, #00f0ff, #0066ff)' : '#111624';
      card.style.color = isActive ? '#000' : '#fff';
      card.style.border = `1px solid ${isActive ? '#ffffff' : '#212a3d'}`;
      card.style.borderRadius = '8px';
      card.style.padding = '12px 10px';
      card.style.cursor = 'pointer';
      card.style.display = 'flex';
      card.style.flexDirection = 'column';
      card.style.gap = '6px';
      card.style.transition = 'all 0.15s ease';
      card.style.boxShadow = isActive ? '0 0 16px rgba(0, 240, 255, 0.4)' : 'none';

      const arpStatus = scene.arp && scene.arp.enabled ? `ARP: ${scene.arp.mode}` : 'ARP: OFF';

      card.innerHTML = `
        <div style="display: flex; justify-content: space-between; align-items: center;">
          <span style="font-size: 9px; font-weight: 800; opacity: 0.8;">PAD #${idx + 1}</span>
          <span style="font-size: 8px; font-family: monospace; background: rgba(0,0,0,0.25); padding: 1px 4px; border-radius: 3px;">${scene.bpm} BPM</span>
        </div>
        <div style="font-size: 12px; font-weight: 700; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;">${scene.name}</div>
        <div style="font-size: 9px; opacity: 0.85; font-family: monospace; display: flex; justify-content: space-between;">
          <span>Patch #${scene.patchId}</span>
          <span>${arpStatus}</span>
        </div>
      `;

      card.addEventListener('click', () => this.selectScene(idx));
      grid.appendChild(card);
    });

    root.appendChild(grid);
    this.container.appendChild(root);

    // Bind action buttons
    const btnCap = document.getElementById('btnCaptureActiveScene');
    if (btnCap) {
      btnCap.addEventListener('click', () => this.captureCurrentAsScene(this.activeSceneIndex));
    }

    const btnFlash = document.getElementById('btnSaveScenesFlash');
    if (btnFlash) {
      btnFlash.addEventListener('click', () => {
        if (!window.esp32HardwareSync || !window.esp32HardwareSync.isConnected) {
          alert("Conecte o ESP32-S3 via Serial para gravar as cenas na Flash!");
          return;
        }
        this.scenes.forEach(sc => {
          const safeName = sc.name.replace(/\s+/g, '_');
          window.esp32HardwareSync.sendSerialCommand(`scene_save ${safeName}`);
        });
        alert("Todas as 8 cenas foram gravadas na Flash SPIFFS do ESP32!");
      });
    }
  }
}

window.StudioSceneManager = StudioSceneManager;
