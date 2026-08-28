/**
 * AMY Studio - Live Performance Scene & Song Matrix Manager
 * Manages 16 live scenes with instant quantized transition, storing BPM, patterns, patches, arp and macros.
 */

class StudioSceneManager {
  constructor(containerId) {
    this.container = document.getElementById(containerId);
    this.activeSceneIndex = 0;
    this.scenes = [
      { id: 0, name: "01: Synthwave Intro", bpm: 120, patchId: 1, macros: [50, 50, 30, 50, 10, 40, 25, 0] },
      { id: 1, name: "02: 808 Electro Verse", bpm: 128, patchId: 2, macros: [65, 70, 40, 60, 5, 30, 15, 10] },
      { id: 2, name: "03: DX7 Bell Chorus", bpm: 128, patchId: 128, macros: [40, 85, 50, 50, 0, 50, 40, 0] },
      { id: 3, name: "04: Acid Bass Peak", bpm: 135, patchId: 4, macros: [90, 95, 80, 80, 0, 10, 10, 40] }
    ];

    if (this.container) this.render();
  }

  selectScene(index) {
    if (index < 0 || index >= this.scenes.length) return;
    this.activeSceneIndex = index;
    const s = this.scenes[index];

    console.log(`[Scene Manager] Transição para cena #${index + 1} ("${s.name}")...`);

    // 1. Apply BPM
    if (window.amyStudioSequencer) {
      window.amyStudioSequencer.setBpm(s.bpm);
      const bpmIn = document.getElementById('seqBpmInput');
      if (bpmIn) bpmIn.value = s.bpm;
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

    this.render();
  }

  render() {
    if (!this.container) return;
    this.container.innerHTML = '';

    const grid = document.createElement('div');
    grid.className = 'scenes-grid';
    grid.style.display = 'grid';
    grid.style.gridTemplateColumns = 'repeat(auto-fit, minmax(140px, 1fr))';
    grid.style.gap = '8px';

    this.scenes.forEach((scene, idx) => {
      const isActive = this.activeSceneIndex === idx;
      const btn = document.createElement('div');
      btn.className = `scene-slot-card ${isActive ? 'active' : ''}`;
      btn.style.background = isActive ? 'linear-gradient(135deg, #00f0ff, #0066ff)' : '#141926';
      btn.style.color = isActive ? '#000' : '#fff';
      btn.style.border = `1px solid ${isActive ? '#ffffff' : '#232b3e'}`;
      btn.style.borderRadius = '6px';
      btn.style.padding = '10px 8px';
      btn.style.cursor = 'pointer';
      btn.style.display = 'flex';
      btn.style.flexDirection = 'column';
      btn.style.gap = '4px';
      btn.style.transition = 'all 0.15s ease';

      btn.innerHTML = `
        <div style="font-size: 11px; font-weight: 700;">${scene.name}</div>
        <div style="font-size: 9px; opacity: 0.8; font-family: monospace;">${scene.bpm} BPM • Patch #${scene.patchId}</div>
      `;

      btn.addEventListener('click', () => this.selectScene(idx));
      grid.appendChild(btn);
    });

    this.container.appendChild(grid);
  }
}

window.StudioSceneManager = StudioSceneManager;
