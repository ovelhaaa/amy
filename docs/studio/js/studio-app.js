/**
 * AMY Studio - Main Application Controller & UI Binder
 */

document.addEventListener('DOMContentLoaded', async () => {
  console.log("[AMY Studio] Initializing workstation interface...");

  // 1. Initialize Visualizers & OLED Engine
  window.amyVisualizerEngine.init('oscilloscopeCanvas', 'spectrumCanvas', 'oledDisplayCanvas');

  // 2. Initialize Virtual Piano Keyboard
  window.studioKeyboard.init('pianoKeyboardContainer');

  // 3. Initialize Factory Patches Selector
  populatePatchDropdown();

  // 4. Initialize Rotary Knobs & Sliders Interaction
  setupKnobInteractions();

  // 5. Initialize Sequencer Grid
  setupSequencerGrid();

  // 6. Setup Navigation Tabs
  setupViewTabs();

  // 7. Setup Action Buttons & Hardware Sync
  setupHeaderControls();

  // 8. Auto-initialize AMY WebAssembly engine
  if (window.amyAudioBridge) {
    await window.amyAudioBridge.init();
  }

  // 9. Subscribe to State Changes to update UI
  window.synthStateManager.subscribe((patch, changedProp, source) => {
    updateUiFromState(patch, changedProp);
  });

  // 10. Setup Terminal Input
  setupTerminal();

  // Setup click-to-start audio unlock
  document.body.addEventListener('click', () => {
    if (window.amyAudioBridge && !window.amyAudioBridge.isPlaying) {
      window.amyAudioBridge.startAudio();
    }
  }, { once: true });
});

// ══════════════════════════════════════════════════════════
// PATCH SELECTOR & LIBRARY
// ══════════════════════════════════════════════════════════
function populatePatchDropdown() {
  const select = document.getElementById('patchSelect');
  if (!select) return;
  select.innerHTML = '';

  const groups = {};
  AMY_FACTORY_PATCHES.forEach(p => {
    if (!groups[p.category]) groups[p.category] = [];
    groups[p.category].push(p);
  });

  for (const cat in groups) {
    const optgroup = document.createElement('optgroup');
    optgroup.label = cat;
    groups[cat].forEach(p => {
      const opt = document.createElement('option');
      opt.value = p.id;
      opt.innerText = `#${String(p.id).padStart(3, '0')} - ${p.name}`;
      optgroup.appendChild(opt);
    });
    select.appendChild(optgroup);
  }

  select.addEventListener('change', (e) => {
    const patchId = parseInt(e.target.value);
    window.synthStateManager.loadFactoryPatch(patchId);
  });
}

// ══════════════════════════════════════════════════════════
// ROTARY KNOB & SLIDER INTERACTIONS
// ══════════════════════════════════════════════════════════
function setupKnobInteractions() {
  const knobs = document.querySelectorAll('.knob-container');

  knobs.forEach(knob => {
    let startY = 0;
    let startVal = 0;
    const param = knob.dataset.param;
    const min = parseFloat(knob.dataset.min || 0);
    const max = parseFloat(knob.dataset.max || 100);
    const isMacro = knob.dataset.macro !== undefined;
    const macroIdx = parseInt(knob.dataset.macro);

    const onMouseMove = (e) => {
      const deltaY = startY - e.clientY;
      const range = max - min;
      const step = range / 150.0;
      let newVal = Math.max(min, Math.min(max, startVal + deltaY * step));

      // Update Knob visual angle (-135deg to +135deg)
      const norm = (newVal - min) / range;
      const angle = -135 + norm * 270;
      const ptr = knob.querySelector('.knob-pointer');
      if (ptr) ptr.style.transform = `rotate(${angle}deg)`;

      // Update Value Label
      const valLabel = document.getElementById(`val_${param}`);
      if (valLabel) {
        valLabel.innerText = newVal >= 100 ? newVal.toFixed(0) : newVal.toFixed(1);
      }

      // Update Synth State
      if (isMacro) {
        window.synthStateManager.setMacro(macroIdx, newVal);
      } else {
        window.synthStateManager.setParam(param, newVal, false);
      }
    };

    const onMouseUp = () => {
      window.removeEventListener('mousemove', onMouseMove);
      window.removeEventListener('mouseup', onMouseUp);
    };

    knob.addEventListener('mousedown', (e) => {
      e.preventDefault();
      startY = e.clientY;
      startVal = parseFloat(knob.dataset.current || ((min + max) / 2));
      window.addEventListener('mousemove', onMouseMove);
      window.addEventListener('mouseup', onMouseUp);
    });
  });

  // Range Sliders
  document.querySelectorAll('input[type="range"]').forEach(slider => {
    slider.addEventListener('input', (e) => {
      const param = e.target.dataset.param;
      const val = parseFloat(e.target.value);
      if (param) {
        window.synthStateManager.setParam(param, val);
      }
    });
  });
}

// ══════════════════════════════════════════════════════════
// SEQUENCER GRID SETUP
// ══════════════════════════════════════════════════════════
function setupSequencerGrid() {
  const container = document.getElementById('drumTracksContainer');
  if (!container) return;
  container.innerHTML = '';

  const seq = window.amyStudioSequencer;

  seq.drumTracks.forEach((track, tIdx) => {
    const row = document.createElement('div');
    row.className = 'sequencer-track-row';

    const header = document.createElement('div');
    header.className = 'track-header';
    header.innerText = track.name;
    row.appendChild(header);

    const stepsBox = document.createElement('div');
    stepsBox.className = 'track-steps-container';

    for (let s = 0; s < 16; s++) {
      const stepBtn = document.createElement('div');
      stepBtn.className = `seq-step ${track.pattern[s] ? 'active' : ''}`;
      stepBtn.id = `drum_step_${tIdx}_${s}`;
      stepBtn.innerText = track.pattern[s] ? '●' : '';

      stepBtn.addEventListener('click', () => {
        seq.toggleDrumStep(tIdx, s);
        stepBtn.classList.toggle('active');
        stepBtn.innerText = stepBtn.classList.contains('active') ? '●' : '';
      });

      stepsBox.appendChild(stepBtn);
    }

    row.appendChild(stepsBox);
    container.appendChild(row);
  });

  // Step indicator callback
  seq.onStepChange = (currentStep) => {
    document.querySelectorAll('.seq-step').forEach(el => el.classList.remove('current'));
    for (let t = 0; t < seq.drumTracks.length; t++) {
      const el = document.getElementById(`drum_step_${t}_${currentStep}`);
      if (el) el.classList.add('current');
    }
  };
}

// ══════════════════════════════════════════════════════════
// NAVIGATION TABS
// ══════════════════════════════════════════════════════════
function setupViewTabs() {
  const tabBtns = document.querySelectorAll('.tab-button');
  const viewContainers = document.querySelectorAll('.view-container');

  tabBtns.forEach(btn => {
    btn.addEventListener('click', () => {
      const viewId = btn.dataset.view;
      tabBtns.forEach(b => b.classList.remove('active'));
      viewContainers.forEach(c => c.classList.remove('active'));

      btn.classList.add('active');
      const targetView = document.getElementById(viewId);
      if (targetView) targetView.classList.add('active');

      // Update OLED display simulator screen
      if (viewId === 'viewHardware') {
        window.amyVisualizerEngine.currentOledScreen = "System";
      } else if (viewId === 'viewSequencer') {
        window.amyVisualizerEngine.currentOledScreen = "Sequencer";
      } else if (viewId === 'viewMidi') {
        window.amyVisualizerEngine.currentOledScreen = "MidiMonitor";
      } else {
        window.amyVisualizerEngine.currentOledScreen = "Home";
      }
    });
  });
}

// ══════════════════════════════════════════════════════════
// HEADER ACTIONS & HARDWARE SYNC
// ══════════════════════════════════════════════════════════
function setupHeaderControls() {
  // Panic Button
  document.getElementById('btnPanic')?.addEventListener('click', () => {
    if (window.amyAudioBridge) window.amyAudioBridge.panic();
  });

  // Play / Stop Sequencer Button
  document.getElementById('btnSeqPlay')?.addEventListener('click', (e) => {
    const seq = window.amyStudioSequencer;
    seq.toggle();
    e.target.innerText = seq.isPlaying ? '⏹ STOP' : '▶ PLAY';
    e.target.classList.toggle('btn-danger', seq.isPlaying);
    e.target.classList.toggle('btn-primary', !seq.isPlaying);
  });

  // Undo / Redo
  document.getElementById('btnUndo')?.addEventListener('click', () => window.synthStateManager.undo());
  document.getElementById('btnRedo')?.addEventListener('click', () => window.synthStateManager.redo());

  // Connect ESP32 Button
  document.getElementById('btnConnectEsp32')?.addEventListener('click', async () => {
    const sync = window.esp32HardwareSync;
    if (sync.isConnected) {
      await sync.disconnectSerial();
      document.getElementById('btnConnectEsp32').innerText = '🔌 CONECTAR ESP32';
      document.getElementById('btnConnectEsp32').classList.remove('btn-danger');
      document.getElementById('btnConnectEsp32').classList.add('btn-primary');
    } else {
      const ok = await sync.connectSerial();
      if (ok) {
        document.getElementById('btnConnectEsp32').innerText = '🔌 DESCONECTAR';
        document.getElementById('btnConnectEsp32').classList.add('btn-danger');
        document.getElementById('btnConnectEsp32').classList.remove('btn-primary');
      }
    }
  });

  // Upload Patch to ESP32 Flash Button
  document.getElementById('btnUploadFlash')?.addEventListener('click', () => {
    const p = window.synthStateManager.currentPatch;
    window.esp32HardwareSync.uploadPatchToFlash(p.id, p);
  });

  // Export Patch JSON (.amy)
  document.getElementById('btnExportPatch')?.addEventListener('click', () => {
    const patch = window.synthStateManager.currentPatch;
    const dataStr = "data:text/json;charset=utf-8," + encodeURIComponent(JSON.stringify(patch, null, 2));
    const a = document.createElement('a');
    a.href = dataStr;
    a.download = `${patch.name.replace(/\s+/g, '_').toLowerCase()}.amy`;
    a.click();
  });

  // Import Patch JSON
  document.getElementById('fileImportPatch')?.addEventListener('change', (e) => {
    const file = e.target.files[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = (evt) => {
      try {
        const loaded = JSON.parse(evt.target.result);
        window.synthStateManager.currentPatch = loaded;
        window.synthStateManager.applyFullPatch();
        window.synthStateManager.notify('all', 'import');
        alert(`Patch "${loaded.name}" carregado com sucesso!`);
      } catch (err) {
        alert("Erro ao importar arquivo de patch: " + err.message);
      }
    };
    reader.readAsText(file);
  });
}

// ══════════════════════════════════════════════════════════
// TERMINAL LOG & CLI
// ══════════════════════════════════════════════════════════
function setupTerminal() {
  const input = document.getElementById('terminalInput');
  if (!input) return;

  input.addEventListener('keydown', (e) => {
    if (e.key === 'Enter') {
      const cmd = input.value.trim();
      if (cmd.length > 0) {
        logTerminal(`smk> ${cmd}`, 'info');
        window.esp32HardwareSync.sendSerialCommand(cmd);
        input.value = '';
      }
    }
  });

  window.esp32HardwareSync.onLogMessage = (msg, type) => {
    logTerminal(msg, type);
  };
}

function logTerminal(msg, type = 'info') {
  const win = document.getElementById('terminalOutput');
  if (!win) return;
  const line = document.createElement('div');
  line.className = `terminal-line ${type}`;
  line.innerText = `[${new Date().toLocaleTimeString()}] ${msg}`;
  win.appendChild(line);
  win.scrollTop = win.scrollHeight;
}

function updateUiFromState(patch, changedProp) {
  // Update dropdown if changed
  const select = document.getElementById('patchSelect');
  if (select && select.value != patch.id) {
    select.value = patch.id;
  }
}
