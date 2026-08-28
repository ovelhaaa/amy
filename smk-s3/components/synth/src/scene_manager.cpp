#include "scene_manager.h"
#include "patch_manager.h"
#include "clock_manager.h"
#include "arpeggiator.h"
#include "step_sequencer.h"
#include "pad_bank.h"
#include "storage_manager.h"
#include "ui_manager.h"
#include "esp_log.h"
#include <algorithm>
#include <cstdio>

static const char* TAG = "SceneManager";

namespace smk {

SceneManager::SceneManager() {
    initFactoryScenes();
}

void SceneManager::initFactoryScenes() {
    const char* default_names[kMaxScenes] = {
        "01: INTRO",
        "02: VERSE",
        "03: PRE-CHORUS",
        "04: CHORUS",
        "05: BRIDGE",
        "06: SOLO",
        "07: BREAKDOWN",
        "08: OUTRO"
    };

    for (size_t i = 0; i < kMaxScenes; ++i) {
        auto& sc = scenes_[i];
        memset(&sc, 0, sizeof(Scene));
        snprintf(sc.name, sizeof(sc.name), "%s", default_names[i]);
        sc.patch_id = static_cast<uint8_t>(i + 1); // Patches 1..8
        sc.bpm = (i == 2 || i == 3) ? 124.0f : ((i == 4) ? 118.0f : ((i == 6) ? 110.0f : 120.0f));
        sc.knob_bank = static_cast<uint8_t>(KnobBank::BankA_Macros);
        sc.pad_bank = (i == 0 || i == 6) ? static_cast<uint8_t>(PadBank::BankD_Performance) : static_cast<uint8_t>(PadBank::BankA_Drums);
        
        for (uint8_t m = 0; m < 8; ++m) {
            sc.macro_values[m] = 50.0f;
        }

        sc.arp_enabled = (i == 0 || i == 2 || i == 4 || i == 6);
        sc.arp_mode = (i == 2) ? static_cast<uint8_t>(ArpMode::UpDown) : ((i == 4) ? static_cast<uint8_t>(ArpMode::Random) : static_cast<uint8_t>(ArpMode::Up));
        sc.arp_division = static_cast<uint8_t>(ArpDivision::Div1_16);
        sc.arp_octaves = (i == 3 || i == 5) ? 2 : 1;
        sc.arp_latch = false;
        sc.seq_playing = (i > 0 && i < 7);
        sc.drum_pattern = static_cast<uint8_t>(i % 8);
        sc.drum_mutes = (i == 0 || i == 6) ? 0x01 : 0x00; // Mute kick in intro/breakdown
        sc.transition_mode = static_cast<uint8_t>(SceneTransition::EndOfPattern);
        sc.crc32 = calculateSceneCrc32(sc);
    }
}

bool SceneManager::begin(PatchManager* patch_mgr,
                         ClockManager* clock_mgr,
                         Arpeggiator* arp,
                         StepSequencer* seq,
                         PadManager* pad_mgr,
                         StorageManager* storage,
                         UIManager* ui_mgr) {
    patch_manager_ = patch_mgr;
    clock_manager_ = clock_mgr;
    arpeggiator_ = arp;
    sequencer_ = seq;
    pad_manager_ = pad_mgr;
    storage_manager_ = storage;
    ui_manager_ = ui_mgr;

    loadAllFromFlash();
    applySceneToSystem(scenes_[active_scene_index_]);

    ESP_LOGI(TAG, "SceneManager initialized with %u scenes", kMaxScenes);
    return true;
}

bool SceneManager::selectScene(uint8_t index, bool immediate) {
    if (index >= kMaxScenes) return false;

    if (immediate || (sequencer_ && !sequencer_->isPlaying())) {
        active_scene_index_ = index;
        pending_scene_index_ = -1;
        applySceneToSystem(scenes_[active_scene_index_]);

        ESP_LOGI(TAG, "Activated Scene #%u [%s]", active_scene_index_ + 1, scenes_[active_scene_index_].name);

        if (ui_manager_) {
            ui_manager_->triggerParameterOverlay("SCENE LOAD", "LIVE", 
                                                 static_cast<float>(active_scene_index_ + 1), 
                                                 0.0f, 
                                                 scenes_[active_scene_index_].name, 
                                                 TakeoverStatus::Captured);
        }
    } else {
        queueScene(index);
    }

    return true;
}

void SceneManager::queueScene(uint8_t index) {
    if (index >= kMaxScenes) return;
    pending_scene_index_ = static_cast<int8_t>(index);
    ESP_LOGI(TAG, "Queued Scene #%u [%s] for next pattern boundary", index + 1, scenes_[index].name);

    if (ui_manager_) {
        ui_manager_->triggerParameterOverlay("SCENE QUEUED", "SYNC", 
                                             static_cast<float>(index + 1), 
                                             0.0f, 
                                             scenes_[index].name, 
                                             TakeoverStatus::ApproachingFromBelow);
    }
}

void SceneManager::processPendingTransition() {
    if (pending_scene_index_ >= 0 && pending_scene_index_ < static_cast<int8_t>(kMaxScenes)) {
        uint8_t next_idx = static_cast<uint8_t>(pending_scene_index_);
        pending_scene_index_ = -1;
        selectScene(next_idx, true);
    }
}

void SceneManager::nextScene() {
    selectScene((active_scene_index_ + 1) % kMaxScenes, false);
}

void SceneManager::previousScene() {
    uint8_t prev = (active_scene_index_ == 0) ? (kMaxScenes - 1) : (active_scene_index_ - 1);
    selectScene(prev, false);
}

void SceneManager::captureCurrentAsScene(uint8_t index, const char* custom_name) {
    if (index >= kMaxScenes) return;

    auto& sc = scenes_[index];
    if (custom_name && strlen(custom_name) > 0) {
        snprintf(sc.name, sizeof(sc.name), "%s", custom_name);
    }

    if (patch_manager_) {
        sc.patch_id = patch_manager_->activePatchId();
        sc.knob_bank = static_cast<uint8_t>(patch_manager_->activeKnobBank());
        for (uint8_t i = 0; i < 8; ++i) {
            sc.macro_values[i] = patch_manager_->activePatch().macros[i].current_val;
        }
    }

    if (clock_manager_) {
        sc.bpm = clock_manager_->bpm();
    }

    if (pad_manager_) {
        sc.pad_bank = static_cast<uint8_t>(pad_manager_->activeBank());
    }

    if (arpeggiator_) {
        sc.arp_enabled = arpeggiator_->isEnabled();
        sc.arp_mode = static_cast<uint8_t>(arpeggiator_->mode());
        sc.arp_division = static_cast<uint8_t>(arpeggiator_->division());
        sc.arp_octaves = arpeggiator_->octaves();
        sc.arp_latch = arpeggiator_->latch();
    }

    if (sequencer_) {
        sc.seq_playing = sequencer_->isPlaying();
        sc.drum_pattern = sequencer_->currentPattern();
        sc.drum_mutes = 0;
        for (uint8_t t = 0; t < 4; ++t) {
            if (sequencer_->isTrackMuted(t)) {
                sc.drum_mutes |= (1 << t);
            }
        }
    }

    sc.crc32 = calculateSceneCrc32(sc);
    saveSceneToFlash(index);

    ESP_LOGI(TAG, "Captured live state into Scene #%u [%s]", index + 1, sc.name);

    if (ui_manager_) {
        ui_manager_->triggerParameterOverlay("SCENE SAVED", "FLASH", 
                                             static_cast<float>(index + 1), 
                                             0.0f, 
                                             sc.name, 
                                             TakeoverStatus::Captured);
    }
}

void SceneManager::applySceneToSystem(const Scene& sc) {
    if (patch_manager_) {
        patch_manager_->selectPatch(sc.patch_id);
        patch_manager_->setKnobBank(static_cast<KnobBank>(sc.knob_bank));
        for (uint8_t i = 0; i < 8; ++i) {
            patch_manager_->setMacro(i, sc.macro_values[i], false);
        }
    }

    if (clock_manager_) {
        clock_manager_->setBpm(sc.bpm);
    }

    if (pad_manager_) {
        pad_manager_->setBank(static_cast<PadBank>(sc.pad_bank));
    }

    if (arpeggiator_) {
        arpeggiator_->setEnabled(sc.arp_enabled);
        arpeggiator_->setMode(static_cast<ArpMode>(sc.arp_mode));
        arpeggiator_->setDivision(static_cast<ArpDivision>(sc.arp_division));
        arpeggiator_->setOctaves(sc.arp_octaves);
        arpeggiator_->setLatch(sc.arp_latch);
    }

    if (sequencer_) {
        sequencer_->selectPattern(sc.drum_pattern);
        for (uint8_t t = 0; t < 4; ++t) {
            sequencer_->setTrackMute(t, (sc.drum_mutes & (1 << t)) != 0);
        }
        if (sc.seq_playing && !sequencer_->isPlaying()) {
            sequencer_->play();
        } else if (!sc.seq_playing && sequencer_->isPlaying()) {
            sequencer_->stop();
        }
    }
}

bool SceneManager::saveSceneToFlash(uint8_t index) {
    if (index >= kMaxScenes || !storage_manager_) return false;
    char name_buf[16];
    snprintf(name_buf, sizeof(name_buf), "slot_%u", index + 1);
    return storage_manager_->saveScene(name_buf, scenes_[index]);
}

bool SceneManager::loadSceneFromFlash(uint8_t index) {
    if (index >= kMaxScenes || !storage_manager_) return false;
    char name_buf[16];
    snprintf(name_buf, sizeof(name_buf), "slot_%u", index + 1);
    Scene loaded = {};
    if (storage_manager_->loadScene(name_buf, loaded)) {
        scenes_[index] = loaded;
        return true;
    }
    return false;
}

bool SceneManager::saveAllToFlash() {
    bool ok = true;
    for (uint8_t i = 0; i < kMaxScenes; ++i) {
        if (!saveSceneToFlash(i)) ok = false;
    }
    return ok;
}

bool SceneManager::loadAllFromFlash() {
    bool any_loaded = false;
    for (uint8_t i = 0; i < kMaxScenes; ++i) {
        if (loadSceneFromFlash(i)) any_loaded = true;
    }
    return any_loaded;
}

const Scene& SceneManager::scene(uint8_t index) const {
    if (index >= kMaxScenes) return scenes_[0];
    return scenes_[index];
}

const Scene& SceneManager::activeScene() const {
    return scenes_[active_scene_index_];
}

void SceneManager::setSceneName(uint8_t index, const char* name) {
    if (index >= kMaxScenes || !name) return;
    snprintf(scenes_[index].name, sizeof(scenes_[index].name), "%s", name);
    scenes_[index].crc32 = calculateSceneCrc32(scenes_[index]);
}

} // namespace smk
