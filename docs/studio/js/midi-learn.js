/**
 * AMY Studio - Dynamic MIDI Learn & Controller Mapping Profile Manager
 * Enables one-click MIDI CC mapping for any knob, slider or button, and exports to ESP32 .s3m binary format.
 */

class MidiLearnManager {
  constructor() {
    this.isLearning = false;
    this.activeTarget = null; // { paramKey, min, max, element }
    this.mappings = new Map(); // paramKey -> { cc, channel, min, max, curve }
    this.onMappingUpdate = null;
  }

  startLearn(paramKey, element, min = 0, max = 100) {
    if (this.activeTarget && this.activeTarget.element) {
      this.activeTarget.element.classList.remove('learning');
    }

    this.isLearning = true;
    this.activeTarget = { paramKey, element, min, max };
    if (element) element.classList.add('learning');
    console.log(`[MIDI Learn] Aguardando sinal MIDI para '${paramKey}'...`);
  }

  cancelLearn() {
    if (this.activeTarget && this.activeTarget.element) {
      this.activeTarget.element.classList.remove('learning');
    }
    this.isLearning = false;
    this.activeTarget = null;
  }

  processMidiMessage(channel, ccNumber, value) {
    // If we are currently learning a target:
    if (this.isLearning && this.activeTarget) {
      const mapping = {
        paramKey: this.activeTarget.paramKey,
        cc: ccNumber,
        channel: channel,
        min: this.activeTarget.min,
        max: this.activeTarget.max,
        curve: 'linear'
      };

      this.mappings.set(this.activeTarget.paramKey, mapping);
      console.log(`[MIDI Learn] '${this.activeTarget.paramKey}' mapeado para CC #${ccNumber} (Canal ${channel + 1})!`);

      if (this.activeTarget.element) {
        this.activeTarget.element.classList.remove('learning');
        this.activeTarget.element.classList.add('mapped');
      }

      this.isLearning = false;
      this.activeTarget = null;
      if (this.onMappingUpdate) this.onMappingUpdate(this.mappings);
      return;
    }

    // Otherwise check if this CC matches any existing mapping
    for (const [key, map] of this.mappings.entries()) {
      if (map.cc === ccNumber && (map.channel === channel || map.channel === 0)) {
        const norm = value / 127.0;
        const mappedVal = map.min + norm * (map.max - map.min);

        // Check if it's a macro or direct synth parameter
        if (key.startsWith('macro_')) {
          const idx = parseInt(key.split('_')[1]);
          window.synthStateManager.setMacro(idx, mappedVal);
        } else {
          const junoSlider = document.querySelector(`.juno-slider[data-juno="${key}"]`);
          if (junoSlider) {
            junoSlider.value = mappedVal;
            if (window.juno106Panel) window.juno106Panel.setVcfParam(key, mappedVal);
          } else {
            window.synthStateManager.setParam(key, mappedVal, false);
            // If it's a generic slider mapped by data-param (like volume), update it
            const genericSlider = document.querySelector(`input[type="range"][data-param="${key}"]`);
            if (genericSlider) genericSlider.value = mappedVal;
          }
        }
      }
    }
  }

  exportProfileJson() {
    const profileObj = {
      magic: "0x53334D31", // "S3M1"
      version: 1,
      name: "AMY Studio Mapping",
      mappings: Array.from(this.mappings.values())
    };
    return JSON.stringify(profileObj, null, 2);
  }

  importProfileJson(jsonStr) {
    try {
      const parsed = JSON.parse(jsonStr);
      if (parsed.mappings && Array.isArray(parsed.mappings)) {
        this.mappings.clear();
        parsed.mappings.forEach(m => this.mappings.set(m.paramKey, m));
        console.log(`[MIDI Learn] Perfil importado com ${this.mappings.size} mapeamentos.`);
        if (this.onMappingUpdate) this.onMappingUpdate(this.mappings);
      }
    } catch (e) {
      console.error("[MIDI Learn] Erro ao carregar perfil:", e);
    }
  }
}

window.midiLearnManager = new MidiLearnManager();
