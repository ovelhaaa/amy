/**
 * AMY Studio - Main Application Controller & UI Binder
 * Integrates Synthesis, DX7 Matrix, Juno-106, Graphical Envelopes, Sequencer, Piano Roll, PCM Browser, Scenes & WAV Recorder.
 */

document.addEventListener('DOMContentLoaded', async () => {
  console.log("[AMY Studio] Initializing workstation interface...");

  // 1. Initialize Visualizers & OLED Engine
  window.amyVisualizerEngine.init('oscilloscopeCanvas', 'spectrumCanvas', 'oledDisplayCanvas');

  // 2. Initialize Virtual Piano Keyboard
  window.studioKeyboard.init('pianoKeyboardContainer');

  // 3. Initialize Interactive Graphical Envelopes
  window.eg0Canvas = new InteractiveEnvelopeCanvas('eg0Canvas', 0);
  window.eg1Canvas = new InteractiveEnvelopeCanvas('eg1Canvas', 1);

  // 4. Initialize Additive Synthesis, Mod Matrix & Layer/Split
  window.additiveHarmonicEditor = new AdditiveHarmonicEditor('additiveDrawbarsContainer', 'additiveWaveformCanvas');
  window.modMatrixManager = new ModulationMatrixManager('modMatrixContainer');
  window.layerSplitManager = new LayerSplitManager('layerSplitContainer');

  // 5. Initialize Piano Roll & Sample Browser & Scenes
  window.melodicPianoRoll = new MelodicPianoRoll('pianoRollContainer');
  window.sampleBrowser = new SampleBrowserManager('sampleBrowserContainer');
  window.sceneManager = new StudioSceneManager('scenesMatrixContainer');

  // 5b. Initialize M-VAVE SMK25 V2 8-Pad Performance Bank
  window.padBankManager = new PadBankManager('padBankContainer');

  // 6. Initialize Factory Patches Selector
  populatePatchDropdown();

  // 7. Initialize Rotary Knobs & Sliders Interaction
  setupKnobInteractions();

  // 7. Initialize Sequencer Grid
  setupSequencerGrid();

  // 8. Setup Navigation Tabs
  setupViewTabs();

  // 9. Setup Action Buttons, Recorder & Hardware Sync
  setupHeaderControls();

  // 9b. Setup Performance HUD & Global DAW Shortcuts
  setupPerformanceHud();
  setupGlobalShortcuts();

  // 10. Setup Juno-106 Controls
  setupJunoControls();

  // 11. Setup DX7 SysEx Importer
  setupDx7SysExImporter();

  // 12. Setup MIDI Learn Controller
  setupMidiLearn();

  // 13. Setup Terminal & Telemetry
  setupTerminal();

  // 14. Auto-initialize AMY WebAssembly engine
  if (window.amyAudioBridge) {
    await window.amyAudioBridge.init();
  }

  // 15. Subscribe to State Changes to update UI & Envelopes
  window.synthStateManager.subscribe((patch, changedProp, source) => {
    updateUiFromState(patch, changedProp);
  });

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
      if (e.altKey || e.button === 2) {
        e.preventDefault();
        window.midiLearnManager.startLearn(param, knob, min, max);
        return;
      }

      e.preventDefault();
      startY = e.clientY;
      startVal = parseFloat(knob.dataset.current || ((min + max) / 2));
      window.addEventListener('mousemove', onMouseMove);
      window.addEventListener('mouseup', onMouseUp);
    });

    knob.addEventListener('contextmenu', (e) => {
      e.preventDefault();
      window.midiLearnManager.startLearn(param, knob, min, max);
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

    const triggerMidiLearn = (e) => {
      e.preventDefault();
      const param = slider.dataset.param || slider.dataset.juno;
      const min = parseFloat(slider.min || 0);
      const max = parseFloat(slider.max || 100);
      if (param) window.midiLearnManager.startLearn(param, slider, min, max);
    };

    slider.addEventListener('mousedown', (e) => {
      if (e.altKey || e.button === 2) triggerMidiLearn(e);
    });
    slider.addEventListener('contextmenu', triggerMidiLearn);
  });
}

// ══════════════════════════════════════════════════════════
// ROLAND JUNO-106 PANEL BINDINGS
// ══════════════════════════════════════════════════════════
function setupJunoControls() {
  document.querySelectorAll('.juno-slider').forEach(slider => {
    slider.addEventListener('input', (e) => {
      const param = e.target.dataset.juno;
      const val = parseFloat(e.target.value);
      if (param && window.juno106Panel) {
        window.juno106Panel.setVcfParam(param, val);
      }
    });
  });

  document.querySelectorAll('.btn-chorus').forEach(btn => {
    btn.addEventListener('click', () => {
      document.querySelectorAll('.btn-chorus').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      const mode = btn.dataset.chorusMode;
      window.juno106Panel.setChorus(mode);
    });
  });
}

// ══════════════════════════════════════════════════════════
// YAMAHA DX7 SYSEX IMPORTER
// ══════════════════════════════════════════════════════════
function setupDx7SysExImporter() {
  const input = document.getElementById('fileImportSyx');
  if (!input) return;

  input.addEventListener('change', (e) => {
    const file = e.target.files[0];
    if (!file) return;

    const reader = new FileReader();
    reader.onload = (evt) => {
      try {
        const bytes = new Uint8Array(evt.target.result);
        const patches = Dx7SysExImporter.parseSysEx(bytes);
        if (patches.length > 0) {
          patches.forEach(p => AMY_FACTORY_PATCHES.push(p));
          populatePatchDropdown();
          window.synthStateManager.loadFactoryPatch(patches[0].id);
          alert(`Sucesso: ${patches.length} patch(es) do Yamaha DX7 importados e prontos para tocar!`);
        } else {
          alert("Nenhum patch DX7 válido encontrado no arquivo .syx.");
        }
      } catch (err) {
        alert("Erro ao decodificar SysEx: " + err.message);
      }
    };
    reader.readAsArrayBuffer(file);
  });
}

// ══════════════════════════════════════════════════════════
// MIDI LEARN CONTROLLER
// ══════════════════════════════════════════════════════════
function setupMidiLearn() {
  if (window.amyAudioBridge) {
    const origLog = window.amyAudioBridge.onMidiLog;
    window.amyAudioBridge.onMidiLog = (ev) => {
      if (origLog) origLog(ev);
      if (ev.type === 'MIDI' && ev.data && ev.data.length >= 3) {
        const status = ev.data[0] & 0xF0;
        const channel = ev.data[0] & 0x0F;
        if (status === 0xB0) {
          const cc = ev.data[1];
          const val = ev.data[2];
          window.midiLearnManager.processMidiMessage(channel, cc, val);
        } else if (status === 0x90 && ev.data[2] > 0) {
          const note = ev.data[1];
          const vel = ev.data[2] / 127.0;
          if (window.amyStudioSequencer) {
            window.amyStudioSequencer.addHeldNote(note);
            if (window.amyStudioSequencer.isRecording) {
              window.amyStudioSequencer.recordLiveNote(note, vel);
            }
          }
        } else if (status === 0x80 || (status === 0x90 && ev.data[2] === 0)) {
          const note = ev.data[1];
          if (window.amyStudioSequencer) {
            window.amyStudioSequencer.removeHeldNote(note);
          }
        }
      }
    };
  }
}

// ══════════════════════════════════════════════════════════
// SEQUENCER GRID & MELODIC PIANO ROLL
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

  seq.onStepChange = (currentStep) => {
    // Drum steps highlight
    document.querySelectorAll('.seq-step').forEach(el => el.classList.remove('current'));
    for (let t = 0; t < seq.drumTracks.length; t++) {
      const el = document.getElementById(`drum_step_${t}_${currentStep}`);
      if (el) el.classList.add('current');
    }

    // Melodic Piano Roll highlight & trigger
    if (window.melodicPianoRoll) {
      window.melodicPianoRoll.setPlayhead(currentStep);
      const stepNote = window.melodicPianoRoll.steps[currentStep];
      if (stepNote && window.amyAudioBridge) {
        window.amyAudioBridge.noteOn(0, stepNote.note, stepNote.vel);
        setTimeout(() => {
          if (window.amyAudioBridge) window.amyAudioBridge.noteOff(0, stepNote.note);
        }, 120);
      }
    }
  };

  // Arpeggiator UI Controls
  const btnToggleArp = document.getElementById('btnToggleArp');
  const arpModeSelect = document.getElementById('arpModeSelect');
  const arpDivSelect = document.getElementById('arpDivSelect');
  const arpOctSelect = document.getElementById('arpOctSelect');
  const btnToggleArpLatch = document.getElementById('btnToggleArpLatch');
  const arpSwingSlider = document.getElementById('arpSwingSlider');

  btnToggleArp?.addEventListener('click', () => {
    const isEnabled = !seq.arpEnabled;
    seq.setArpEnabled(isEnabled);
    btnToggleArp.innerText = isEnabled ? '⚡ ARPEGIADOR: ON' : '⚡ ARPEGIADOR: OFF';
    btnToggleArp.classList.toggle('btn-primary', isEnabled);
  });

  arpModeSelect?.addEventListener('change', (e) => {
    seq.setArpMode(e.target.value);
  });

  arpDivSelect?.addEventListener('change', (e) => {
    seq.setArpDivision(e.target.value);
  });

  arpOctSelect?.addEventListener('change', (e) => {
    seq.setArpOctaves(parseInt(e.target.value));
  });

  btnToggleArpLatch?.addEventListener('click', () => {
    const latch = !seq.arpLatch;
    seq.setArpLatch(latch);
    btnToggleArpLatch.innerText = latch ? 'LATCH: ON' : 'LATCH: OFF';
    btnToggleArpLatch.classList.toggle('btn-gold', latch);
  });

  arpSwingSlider?.addEventListener('input', (e) => {
    const val = parseInt(e.target.value);
    seq.setArpSwing(val);
    const lbl = document.getElementById('val_arpSwing');
    if (lbl) lbl.innerText = `${val}%`;
  });

  // Standard MIDI (.mid) Export & Import
  document.getElementById('btnExportMidi')?.addEventListener('click', () => {
    const steps = window.melodicPianoRoll ? window.melodicPianoRoll.steps : [];
    window.standardMidiFileHandler.downloadMidi(seq.drumTracks, steps, seq.bpm);
  });

  document.getElementById('fileImportMidi')?.addEventListener('change', (e) => {
    const file = e.target.files[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = (evt) => {
      try {
        const parsed = window.standardMidiFileHandler.importMidiFile(evt.target.result);
        if (parsed.bpm) {
          seq.setBpm(parsed.bpm);
          const bpmIn = document.getElementById('seqBpmInput');
          if (bpmIn) bpmIn.value = parsed.bpm;
        }
        if (parsed.pianoRollSteps && window.melodicPianoRoll) {
          window.melodicPianoRoll.steps = parsed.pianoRollSteps;
          window.melodicPianoRoll.render();
        }
        if (parsed.drumTracks) {
          parsed.drumTracks.forEach(dt => {
            const track = seq.drumTracks.find(t => t.note === dt.note);
            if (track) track.pattern = dt.pattern;
          });
          setupSequencerGrid();
        }
        alert("Arquivo MIDI (.mid) importado com sucesso para o Sequenciador!");
      } catch (err) {
        alert("Erro ao importar arquivo MIDI: " + err.message);
      }
      e.target.value = '';
    };
    reader.readAsArrayBuffer(file);
  });
}

// ══════════════════════════════════════════════════════════
// NAVIGATION TABS & SWITCHING
// ══════════════════════════════════════════════════════════
function switchView(viewId) {
  const tabBtns = document.querySelectorAll('.tab-button');
  const viewContainers = document.querySelectorAll('.view-container');

  tabBtns.forEach(b => b.classList.remove('active'));
  viewContainers.forEach(c => c.classList.remove('active'));

  const btn = document.querySelector(`.tab-button[data-view="${viewId}"]`);
  if (btn) btn.classList.add('active');

  const targetView = document.getElementById(viewId);
  if (targetView) targetView.classList.add('active');

  if (viewId === 'viewHardware') {
    window.amyVisualizerEngine.currentOledScreen = "System";
  } else if (viewId === 'viewSequencer') {
    window.amyVisualizerEngine.currentOledScreen = "Sequencer";
  } else if (viewId === 'viewMidi') {
    window.amyVisualizerEngine.currentOledScreen = "MidiMonitor";
  } else {
    window.amyVisualizerEngine.currentOledScreen = "Home";
  }
}

function setupViewTabs() {
  const tabBtns = document.querySelectorAll('.tab-button');

  tabBtns.forEach(btn => {
    btn.addEventListener('click', () => {
      const viewId = btn.dataset.view;
      switchView(viewId);
    });
  });
}

// ══════════════════════════════════════════════════════════
// HEADER ACTIONS, RECORDER & HARDWARE SYNC
// ══════════════════════════════════════════════════════════
function setupHeaderControls() {
  // WAV Audio Recorder Button
  const btnRec = document.getElementById('btnRecordWav');
  if (btnRec) {
    btnRec.addEventListener('click', () => {
      const rec = window.amyAudioRecorder;
      if (rec.isRecording) {
        rec.stopRecording();
        btnRec.innerText = '🔴 GRAVAR WAV';
        btnRec.classList.remove('btn-danger');
      } else {
        rec.startRecording();
        btnRec.innerText = '⏹ GRAVANDO...';
        btnRec.classList.add('btn-danger');
      }
    });
  }

  document.getElementById('btnPanic')?.addEventListener('click', () => {
    // 1. Panic AMY WebAudio Bridge
    if (window.amyAudioBridge) window.amyAudioBridge.panic();

    // 2. Panic connected ESP32-S3 hardware
    if (window.esp32HardwareSync && window.esp32HardwareSync.isConnected) {
      window.esp32HardwareSync.sendSerialCommand('panic');
      window.esp32HardwareSync.sendWireIfConnected('S131072Z');
    }

    // 3. Stop sequencer if running
    if (window.amyStudioSequencer && window.amyStudioSequencer.isPlaying) {
      window.amyStudioSequencer.stop();
      const btnPlay = document.getElementById('btnSeqPlay');
      if (btnPlay) {
        btnPlay.innerText = '▶ PLAY';
        btnPlay.classList.remove('btn-danger');
        btnPlay.classList.add('btn-primary');
      }
    }

    // 4. Clear virtual keyboard active keys
    if (window.studioKeyboard) {
      window.studioKeyboard.activeKeys.forEach(n => window.studioKeyboard.triggerNoteOff(n));
      window.studioKeyboard.activeKeys.clear();
      document.querySelectorAll('.piano-key').forEach(k => k.classList.remove('active'));
    }

    // 5. Clear sequencer held notes
    if (window.amyStudioSequencer) {
      window.amyStudioSequencer.heldNotes = [];
    }
  });

  document.getElementById('btnSeqPlay')?.addEventListener('click', (e) => {
    const seq = window.amyStudioSequencer;
    seq.toggle();
    e.target.innerText = seq.isPlaying ? '⏹ STOP' : '▶ PLAY';
    e.target.classList.toggle('btn-danger', seq.isPlaying);
    e.target.classList.toggle('btn-primary', !seq.isPlaying);
  });

  // Live Step Record Binders
  const updateRecButtons = (isRec) => {
    const b1 = document.getElementById('btnRecordSeq');
    const b2 = document.getElementById('btnRecordSeqView');
    [b1, b2].forEach(btn => {
      if (btn) {
        btn.innerText = isRec ? '⏹ REC ON' : '⏺ REC';
        btn.classList.toggle('btn-danger', isRec);
      }
    });

    const btnPlay = document.getElementById('btnSeqPlay');
    if (btnPlay && window.amyStudioSequencer.isPlaying) {
      btnPlay.innerText = '⏹ STOP';
      btnPlay.classList.add('btn-danger');
      btnPlay.classList.remove('btn-primary');
    }
  };

  if (window.amyStudioSequencer) {
    window.amyStudioSequencer.onRecordStateChange = updateRecButtons;
  }

  document.getElementById('btnRecordSeq')?.addEventListener('click', () => {
    window.amyStudioSequencer.toggleRecord();
  });

  document.getElementById('btnRecordSeqView')?.addEventListener('click', () => {
    window.amyStudioSequencer.toggleRecord();
  });

  document.getElementById('btnUndo')?.addEventListener('click', () => window.synthStateManager.undo());
  document.getElementById('btnRedo')?.addEventListener('click', () => window.synthStateManager.redo());

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

  document.getElementById('btnUploadFlash')?.addEventListener('click', () => {
    const p = window.synthStateManager.currentPatch;
    window.esp32HardwareSync.uploadPatchToFlash(p.id, p);
  });

  document.getElementById('btnExportPatch')?.addEventListener('click', () => {
    const patch = window.synthStateManager.currentPatch;
    const s3pBytes = window.esp32HardwareSync.packPatchToS3P(patch);
    const blob = new Blob([s3pBytes], { type: 'application/octet-stream' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `${patch.name.replace(/\s+/g, '_').toLowerCase()}.s3p`;
    a.click();
    URL.revokeObjectURL(url);
  });

  // Export C++ Factory Table File
  document.getElementById('btnExportCpp')?.addEventListener('click', () => {
    if (window.cppPatchTableGenerator) {
      window.cppPatchTableGenerator.downloadCppFile();
    }
  });

  // Download Complete SPIFFS Backup (.s3b)
  document.getElementById('btnBackupSpiffs')?.addEventListener('click', () => {
    if (window.spiffsBackupManager) {
      window.spiffsBackupManager.downloadBackup();
    }
  });

  // Restore Complete SPIFFS Backup (.s3b)
  document.getElementById('fileRestoreS3b')?.addEventListener('change', (e) => {
    const file = e.target.files[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = async (evt) => {
      try {
        const bundle = await window.spiffsBackupManager.parseBackupFile(evt.target.result);
        await window.spiffsBackupManager.restoreBackup(bundle, true);
      } catch (err) {
        alert("Erro ao restaurar backup .s3b: " + err.message);
      }
      e.target.value = '';
    };
    reader.readAsArrayBuffer(file);
  });

  document.getElementById('fileImportPatch')?.addEventListener('change', (e) => {
    const file = e.target.files[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = (evt) => {
      try {
        let loaded;
        if (file.name.endsWith('.s3p')) {
          const bytes = new Uint8Array(evt.target.result);
          loaded = window.esp32HardwareSync.unpackS3P(bytes);
        } else {
          loaded = JSON.parse(evt.target.result);
        }
        window.synthStateManager.pushState();
        window.synthStateManager.currentPatch = loaded;
        window.synthStateManager.applyFullPatch();
        window.synthStateManager.notify('all', 'import');
        alert(`Patch "${loaded.name}" carregado com sucesso!`);
      } catch (err) {
        alert("Erro ao importar arquivo de patch: " + err.message);
      }
      e.target.value = ''; // Reset input
    };
    if (file.name.endsWith('.s3p')) {
      reader.readAsArrayBuffer(file);
    } else {
      reader.readAsText(file);
    }
  });

  // Connect Web Bluetooth MIDI (BLE MIDI)
  const btnBle = document.getElementById('btnConnectBle');
  if (btnBle && window.bleMidiManager) {
    window.bleMidiManager.onStateChange = (connected, deviceName) => {
      btnBle.innerText = connected ? `📶 ${deviceName || 'BLE CONECTADO'}` : '📶 BLUETOOTH MIDI';
      btnBle.classList.toggle('btn-danger', connected);
      btnBle.classList.toggle('btn-primary', !connected);
    };

    btnBle.addEventListener('click', async () => {
      await window.bleMidiManager.toggleConnect();
    });
  }
}

// ══════════════════════════════════════════════════════════
// FULLSCREEN PERFORMANCE HUD (LIVE STAGE VIEW)
// ══════════════════════════════════════════════════════════
function setupPerformanceHud() {
  const overlay = document.getElementById('performanceHudOverlay');
  const btnOpen = document.getElementById('btnPerformanceMode');
  const btnClose = document.getElementById('btnCloseHud');
  const btnPanic = document.getElementById('btnHudPanic');

  const openHud = () => {
    if (!overlay) return;
    overlay.classList.add('active');
    renderHudMacros();
    renderHudScenes();

    const bpmDisp = document.getElementById('hudBpmDisplay');
    if (bpmDisp && window.amyStudioSequencer) {
      bpmDisp.innerText = Math.round(window.amyStudioSequencer.bpm);
    }

    if (document.documentElement.requestFullscreen) {
      document.documentElement.requestFullscreen().catch(() => {});
    }
  };

  const closeHud = () => {
    if (!overlay) return;
    overlay.classList.remove('active');
    if (document.fullscreenElement && document.exitFullscreen) {
      document.exitFullscreen().catch(() => {});
    }
  };

  btnOpen?.addEventListener('click', openHud);
  btnClose?.addEventListener('click', closeHud);
  btnPanic?.addEventListener('click', () => {
    document.getElementById('btnPanic')?.click();
  });
}

function renderHudMacros() {
  const container = document.getElementById('hudMacrosContainer');
  if (!container || !window.synthStateManager) return;
  container.innerHTML = '';

  const patch = window.synthStateManager.currentPatch;
  const macros = patch.macros || [];

  macros.forEach((m, idx) => {
    const card = document.createElement('div');
    card.className = 'hud-macro-card';
    card.innerHTML = `
      <span class="hud-macro-name">${m.name}</span>
      <div class="knob-container" data-macro="${idx}" data-param="macro_${idx}" data-min="0" data-max="100" data-current="${m.val}" style="width: 54px; height: 54px;">
        <div class="knob-pointer gold" style="transform: rotate(${-135 + (m.val / 100) * 270}deg);"></div>
      </div>
      <span class="hud-macro-val" id="hud_val_macro_${idx}">${Math.round(m.val)}%</span>
    `;
    container.appendChild(card);
  });

  setupKnobInteractions();
}

function renderHudScenes() {
  const container = document.getElementById('hudScenesContainer');
  if (!container || !window.sceneManager) return;
  container.innerHTML = '';

  const scenes = window.sceneManager.scenes || [];
  const activeIdx = window.sceneManager.activeSceneIndex;

  scenes.forEach((sc, idx) => {
    const slot = document.createElement('div');
    slot.className = `hud-scene-slot ${idx === activeIdx ? 'active' : ''}`;
    slot.innerHTML = `
      <span style="font-size: 8px; font-weight: 800; opacity: 0.8;">PAD ${idx + 1}</span>
      <span class="hud-scene-name">${sc.name}</span>
      <span style="font-size: 8px; font-family: monospace;">${sc.bpm} BPM</span>
    `;

    slot.addEventListener('click', () => {
      window.sceneManager.selectScene(idx);
      renderHudScenes();
      const bpmDisp = document.getElementById('hudBpmDisplay');
      if (bpmDisp) bpmDisp.innerText = sc.bpm;
    });

    container.appendChild(slot);
  });
}

// ══════════════════════════════════════════════════════════
// GLOBAL KEYBOARD SHORTCUTS (DAW WORKFLOW)
// ══════════════════════════════════════════════════════════
function setupGlobalShortcuts() {
  window.addEventListener('keydown', (e) => {
    // Ignore keystrokes when typing in inputs or textareas
    if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;

    // 1. Space: Sequencer Play / Stop
    if (e.code === 'Space') {
      e.preventDefault();
      if (window.amyStudioSequencer) {
        window.amyStudioSequencer.toggle();
        const btnPlay = document.getElementById('btnSeqPlay');
        if (btnPlay) {
          btnPlay.innerText = window.amyStudioSequencer.isPlaying ? '⏹ STOP' : '▶ PLAY';
          btnPlay.classList.toggle('btn-danger', window.amyStudioSequencer.isPlaying);
          btnPlay.classList.toggle('btn-primary', !window.amyStudioSequencer.isPlaying);
        }
      }
      return;
    }

    // 2. Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z: Undo & Redo
    if (e.ctrlKey || e.metaKey) {
      if (e.key.toLowerCase() === 'z') {
        e.preventDefault();
        if (e.shiftKey) {
          window.synthStateManager.redo();
        } else {
          window.synthStateManager.undo();
        }
        return;
      } else if (e.key.toLowerCase() === 'y') {
        e.preventDefault();
        window.synthStateManager.redo();
        return;
      }
    }

    // 3. Escape: Panic & Exit HUD
    if (e.code === 'Escape') {
      e.preventDefault();
      const hud = document.getElementById('performanceHudOverlay');
      if (hud && hud.classList.contains('active')) {
        hud.classList.remove('active');
        if (document.fullscreenElement && document.exitFullscreen) {
          document.exitFullscreen().catch(() => {});
        }
      }
      document.getElementById('btnPanic')?.click();
      return;
    }

    // 4. Digits 1..9, 0: Instant Tab Switching
    const tabMap = {
      'Digit1': 'viewSynth',
      'Digit2': 'viewAdditive',
      'Digit3': 'viewJuno',
      'Digit4': 'viewDx7',
      'Digit5': 'viewSequencer',
      'Digit6': 'viewSamples',
      'Digit7': 'viewScenes',
      'Digit8': 'viewEffects',
      'Digit9': 'viewMacros',
      'Digit0': 'viewPads'
    };

    if (tabMap[e.code]) {
      e.preventDefault();
      switchView(tabMap[e.code]);
      return;
    }
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

  window.esp32HardwareSync.onProgressUpdate = (percent) => {
    let barEl = document.getElementById('uploadProgressBar');
    if (percent === null) {
      if (barEl) barEl.remove();
      return;
    }
    
    if (!barEl) {
      barEl = document.createElement('div');
      barEl.id = 'uploadProgressBar';
      barEl.style.width = '100%';
      barEl.style.height = '6px';
      barEl.style.background = '#232b3e';
      barEl.style.marginTop = '8px';
      barEl.style.borderRadius = '3px';
      barEl.innerHTML = `<div id="uploadProgressFill" style="width: 0%; height: 100%; background: #00ff88; border-radius: 3px; transition: width 0.2s;"></div>`;
      
      const win = document.getElementById('terminalOutput');
      if (win) {
        win.appendChild(barEl);
        win.scrollTop = win.scrollHeight;
      }
    }
    
    const fill = document.getElementById('uploadProgressFill');
    if (fill) fill.style.width = `${percent}%`;
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
  const select = document.getElementById('patchSelect');
  if (select && select.value != patch.id) {
    select.value = patch.id;
  }

  if (window.eg0Canvas) {
    window.eg0Canvas.updateParams(patch.amp_attack, patch.amp_decay, patch.amp_sustain, patch.amp_release, patch.eg0_type);
  }
  if (window.eg1Canvas) {
    window.eg1Canvas.updateParams(patch.eg1_attack, patch.eg1_decay, patch.eg1_sustain, patch.eg1_release, patch.eg1_type);
  }

  // Helper to update a single knob
  const updateKnob = (key, val) => {
    const knob = document.querySelector(`[data-param="${key}"]`);
    if (knob) {
      const min = parseFloat(knob.dataset.min || 0);
      const max = parseFloat(knob.dataset.max || 100);
      const pct = Math.max(0, Math.min(1, (val - min) / (max - min)));
      const pointer = knob.querySelector('.knob-pointer');
      if (pointer) {
        const deg = -135 + (pct * 270);
        pointer.style.transform = `rotate(${deg}deg)`;
      }
      const label = document.getElementById(`val_${key}`);
      if (label) {
        label.innerText = (val < 10 && !Number.isInteger(val)) ? val.toFixed(2) : Math.round(val);
      }
    }
  };

  // Update analog params
  Object.keys(patch).forEach(key => {
    updateKnob(key, patch[key]);
    
    // Also update dropdowns (like wave_type, filter_type)
    const selects = document.querySelectorAll('select.patch-dropdown');
    selects.forEach(s => {
      if (s.onchange && s.onchange.toString().includes(`'${key}'`)) {
        s.value = patch[key];
      }
    });
  });

  // Update macros
  if (patch.macros && Array.isArray(patch.macros)) {
    patch.macros.forEach((m, idx) => {
      updateKnob(`macro_${idx}`, m.val);
    });
  }

  // Update DX7 operators if present
  if (patch.operators && Array.isArray(patch.operators)) {
    patch.operators.forEach(op => {
      updateKnob(`fm_op${op.opNum || op.id}_ratio`, op.ratio);
      const ampVal = op.level !== undefined ? op.level : op.amp;
      updateKnob(`fm_op${op.opNum || op.id}_amp`, ampVal);
    });
    updateKnob(`fm_op6_fb`, patch.feedback);
    
    const algoSelect = document.getElementById('dx7AlgoSelect');
    if (algoSelect) algoSelect.value = patch.algorithm;
  }

  // Update Additive Harmonics if present
  if (patch.harmonics && window.additiveHarmonicEditor) {
    window.additiveHarmonicEditor.importData(patch.harmonics);
  }

  // Update Mod Matrix if present
  if (patch.mod_matrix && window.modMatrixManager) {
    window.modMatrixManager.importData(patch.mod_matrix);
  }
}
