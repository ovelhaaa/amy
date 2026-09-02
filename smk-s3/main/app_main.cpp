#include "app_config.h"
#include "diagnostics.h"
#include "console.h"

#include "event_bus.h"
#include "pcm5102_output.h"
#include "amy_adapter.h"
#include "audio_task.h"
#include "usb_midi_host.h"
#include "st7789_display_driver.h"
#include "st7735_display_driver.h"
#include "dummy_display_driver.h"
#include "ui_manager.h"
#include "patch_manager.h"
#include "clock_manager.h"
#include "arpeggiator.h"
#include "step_sequencer.h"
#include "storage_manager.h"
#include "nvs_storage.h"
#include "midi_learn.h"
#include "controller_profile.h"
#include "pad_bank.h"
#include "scene_manager.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_psram.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "Main";

struct SequencerContext {
    smk::Arpeggiator*   arpeggiator;
    smk::StepSequencer* sequencer;
    smk::SceneManager*  scene_manager;
    smk::EventBus*      event_bus;
};

static void onClockTick(uint32_t tick_count, void* ctx) {
    auto* s_ctx = static_cast<SequencerContext*>(ctx);
    if (s_ctx->arpeggiator && s_ctx->event_bus) {
        s_ctx->arpeggiator->processTick(tick_count, *s_ctx->event_bus);
    }
    if (s_ctx->sequencer && s_ctx->event_bus) {
        s_ctx->sequencer->processTick(tick_count, *s_ctx->event_bus);
    }
    if (s_ctx->scene_manager && (tick_count % 24 == 0)) {
        s_ctx->scene_manager->processPendingTransition();
    }
}

static void app_init_task(void* arg) {
    ESP_LOGI(TAG, "=========================================");
    ESP_LOGI(TAG, " Project: %s", smk::config::kProjectName);
    ESP_LOGI(TAG, " Version: %s", smk::config::kFirmwareVersion);
    ESP_LOGI(TAG, "=========================================");

    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        flash_size = 0;
    }

    ESP_LOGI(TAG, "System Info:");
    ESP_LOGI(TAG, "  Chip: ESP32-S3 Rev %d", chip_info.revision);
    ESP_LOGI(TAG, "  Cores: %d", chip_info.cores);
    ESP_LOGI(TAG, "  Flash: %lu MB", flash_size / (1024 * 1024));
    ESP_LOGI(TAG, "  PSRAM: %d MB", esp_psram_get_size() / (1024 * 1024));
    ESP_LOGI(TAG, "  Free Heap (Internal): %zu bytes", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    ESP_LOGI(TAG, "  Free Heap (PSRAM): %zu bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    // 1. Initialize NVS and Flash Storage
    smk::NvsStorage* nvs_storage = new smk::NvsStorage();
    nvs_storage->begin();

    smk::StorageManager* storage_manager = new smk::StorageManager();
    storage_manager->begin();

    // 3. Create EventBus
    smk::EventBus* event_bus = new smk::EventBus(smk::config::kEventQueueCapacity);

    // 4. Create PCM5102Output
    smk::PCM5102Output* pcm_out = new smk::PCM5102Output(
        smk::config::kI2sBclk,
        smk::config::kI2sLrclk,
        smk::config::kI2sData,
        smk::config::kI2sMute
    );

    // 5. Initialize PCM5102 output
    if (!pcm_out->begin()) {
        ESP_LOGE(TAG, "Failed to initialize PCM5102 output");
    } else {
        pcm_out->start();
    }

    // 6. Create AmyAdapter
    smk::AmyAdapter* amy_adapter = new smk::AmyAdapter();

    // 7. Initialize AMY adapter
    if (!amy_adapter->begin(smk::config::kSampleRateHz)) {
        ESP_LOGE(TAG, "Failed to initialize AMY engine");
    }

    // 8. Start AudioTask
    smk::AudioTask::start(amy_adapter, pcm_out, smk::config::kAudioTaskCore, smk::config::kAudioTaskPriority);

    // 9. Initialize UI Subsystem for 1.8" (160x128) ST7735 Display
    smk::ST7735Config st7735_cfg = {
        .mosi_pin = smk::config::kDisplayMosi,
        .sclk_pin = smk::config::kDisplaySclk,
        .cs_pin   = smk::config::kDisplayCs,
        .dc_pin   = smk::config::kDisplayDc,
        .rst_pin  = smk::config::kDisplayRst,
        .bl_pin   = smk::config::kDisplayBl,
        .width    = 160,
        .height   = 128,
        .x_offset = 0,
        .y_offset = 0
    };

    smk::DisplayDriver* display_driver = new smk::ST7735DisplayDriver(st7735_cfg);
    if (!display_driver->begin()) {
        ESP_LOGE(TAG, "ST7735 1.8\" Display initialization failed; using DummyDisplayDriver fallback");
        delete display_driver;
        display_driver = new smk::DummyDisplayDriver(160, 128);
        display_driver->begin();
    }

    smk::UIManager* ui_manager = new smk::UIManager(*display_driver);
    ui_manager->setSynthEngine(amy_adapter);
    if (!ui_manager->begin()) {
        ESP_LOGE(TAG, "Failed to initialize UIManager");
    } else {
        ui_manager->startTask(0, 4); // Core 0, priority 4 (~30 FPS)
    }

    // 10. Initialize PatchManager & 8-Macro Engine
    smk::PatchManager* patch_manager = new smk::PatchManager();
    patch_manager->begin(amy_adapter, ui_manager);

    // 11. Initialize Arpeggiator, StepSequencer, and ClockManager
    smk::Arpeggiator* arpeggiator = new smk::Arpeggiator();
    smk::StepSequencer* step_sequencer = new smk::StepSequencer();
    smk::ClockManager* clock_manager = new smk::ClockManager();

    patch_manager->setClockManager(clock_manager);
    patch_manager->setArpeggiator(arpeggiator);
    patch_manager->setStepSequencer(step_sequencer);

    if (ui_manager) {
        ui_manager->sequencerScreen().setSequencer(step_sequencer);
    }

    smk::PadManager* pad_manager = new smk::PadManager();

    // 12. Initialize SceneManager (8 Performance Scenes)
    smk::SceneManager* scene_manager = new smk::SceneManager();
    scene_manager->begin(patch_manager, clock_manager, arpeggiator, step_sequencer, pad_manager, storage_manager, ui_manager);
    if (ui_manager) {
        ui_manager->sceneScreen().setSceneManager(scene_manager);
    }

    // Ensure normal playable synth mode at boot (Arp and Drum Sequencer off by default)
    arpeggiator->setEnabled(false);
    step_sequencer->stop();

    SequencerContext* seq_ctx = new SequencerContext{ arpeggiator, step_sequencer, scene_manager, event_bus };
    clock_manager->begin();
    clock_manager->setCallback(onClockTick, seq_ctx);

    // Restore saved NVS configuration if present
    smk::SystemConfig sys_cfg = {};
    if (nvs_storage->loadConfig(sys_cfg)) {
        patch_manager->selectPatch(sys_cfg.active_patch_id);
        clock_manager->setBpm(sys_cfg.global_bpm);
        clock_manager->setSwing(sys_cfg.swing_percent);
        step_sequencer->setSwing((float)sys_cfg.swing_percent);
        arpeggiator->setSwing((float)sys_cfg.swing_percent);
        amy_adapter->setSoftLimiter(sys_cfg.soft_limiter != 0);
    }

    // 13. Create UsbMidiHost
    smk::UsbMidiHost* midi_host = new smk::UsbMidiHost(*event_bus);
    midi_host->setVelocityCurve(static_cast<smk::VelocityCurve>(sys_cfg.velocity_curve));
    if (!midi_host->begin()) {
        ESP_LOGE(TAG, "Failed to initialize USB MIDI Host");
    }

    if (ui_manager && patch_manager) {
        ui_manager->homeScreen().setPatchInfo(patch_manager->activePatch().id, patch_manager->activePatch().name, "SYNTH");
        uint8_t m_vals[8];
        for (int i = 0; i < 8; ++i) m_vals[i] = static_cast<uint8_t>(patch_manager->activePatch().macros[i].current_val);
        ui_manager->homeScreen().setMacroValues(m_vals);
        ui_manager->homeScreen().setUsbConnected(midi_host ? midi_host->isDeviceConnected() : false);
        ui_manager->homeScreen().setBpm(clock_manager ? clock_manager->bpm() : 120.0f);
        ui_manager->homeScreen().setActiveVoices(0, patch_manager->activePatch().voice_count);
        ui_manager->systemScreen().setConfigInfo(
            smk::MidiParser::velocityCurveName(static_cast<smk::VelocityCurve>(sys_cfg.velocity_curve)),
            sys_cfg.swing_percent,
            sys_cfg.soft_limiter != 0
        );
    }

    smk::MidiLearn* midi_learn = new smk::MidiLearn();

    smk::ControllerProfile active_profile = smk::ProfileManager::createDefaultSmk25Profile();
    midi_learn->begin(&active_profile);
    if (ui_manager) {
        ui_manager->midiLearnScreen().setMidiLearn(midi_learn);
    }

    // 13. Create Console and start it
    smk::Console* console = new smk::Console();
    console->setUiManager(ui_manager);
    console->setPatchManager(patch_manager);
    console->setClockManager(clock_manager);
    console->setArpeggiator(arpeggiator);
    console->setStepSequencer(step_sequencer);
    console->setStorageManager(storage_manager);
    console->setNvsStorage(nvs_storage);
    console->setMidiLearn(midi_learn);
    console->setPadManager(pad_manager);
    console->setUsbMidiHost(midi_host);
    console->setAmyAdapter(amy_adapter);
    console->setActiveProfilePointer(&active_profile);
    if (!console->begin()) {
        ESP_LOGE(TAG, "Failed to initialize Console");
    }

    ESP_LOGI(TAG, "Initialization complete");

    // 14. Enter main control loop
    TickType_t last_status_time = xTaskGetTickCount();
    const TickType_t status_interval = pdMS_TO_TICKS(5000);

    while (true) {
        smk::SynthEvent event;
        if (event_bus->receive(event, 10)) {
            // Forward event to UI Manager (for MidiMonitor, Pad hit animations, etc.)
            if (ui_manager) {
                ui_manager->processEvent(event);
            }

            // 1. Intercept incoming MIDI if MIDI Learn Wizard is active
            if (midi_learn && midi_learn->isLearning()) {
                if (event.source == smk::EventSource::UsbMidi) {
                    uint8_t msg_type = 0; // 0=Note, 1=CC, 2=PitchBend
                    if (event.type == smk::EventType::ControlChange) msg_type = 1;
                    else if (event.type == smk::EventType::PitchBend) msg_type = 2;

                    if (midi_learn->processIncomingMidi(msg_type, event.channel, event.id, event.value)) {
                        if (ui_manager) {
                            char cap_buf[32];
                            snprintf(cap_buf, sizeof(cap_buf), "CAPTURADO #%u!", event.id);
                            ui_manager->midiLearnScreen().triggerFeedback(cap_buf, smk::DisplayDriver::kColorGreen);
                        }
                        if (midi_learn->isComplete()) {
                            if (storage_manager) {
                                storage_manager->saveProfile("smk25_custom", active_profile);
                                ESP_LOGI(TAG, "Saved profile 'smk25_custom' to SPIFFS");
                            }
                            if (ui_manager) {
                                ui_manager->midiLearnScreen().triggerFeedback("SALVO NA FLASH!", smk::DisplayDriver::kColorGreen);
                                ui_manager->switchScreen(smk::ScreenId::Home);
                                ui_manager->triggerParameterOverlay("MIDI LEARN", "PERFIL SALVO", 0.0f, 0.0f, "", smk::TakeoverStatus::Captured);
                            }
                        }
                    }
                }
                // Bypass normal synth playback and knob tweaks during wizard
                continue;
            }

            // 2. Process incoming USB MIDI CCs and Notes through active ControllerProfile
            if (event.source == smk::EventSource::UsbMidi && (event.type == smk::EventType::ControlChange || event.type == smk::EventType::NoteOn || event.type == smk::EventType::NoteOff)) {
                uint8_t msg_type = (event.type == smk::EventType::ControlChange) ? 1 : 0;
                smk::TargetAction action = smk::ProfileManager::matchBinding(active_profile, msg_type, event.channel, event.id);

                // If on MIDI Learn screen and not yet learning, allow PLAY to start wizard
                if (ui_manager && ui_manager->activeScreenId() == smk::ScreenId::MidiLearn && !midi_learn->isLearning()) {
                    if (action == smk::TargetAction::Play) {
                        midi_learn->begin(&active_profile);
                        midi_learn->startWizard();
                        ui_manager->midiLearnScreen().triggerFeedback("WIZARD INICIADO!", smk::DisplayDriver::kColorGreen);
                        continue;
                    }
                }

                if (action >= smk::TargetAction::Knob1 && action <= smk::TargetAction::Knob16) {
                    uint8_t knob_idx = static_cast<uint8_t>(action) - static_cast<uint8_t>(smk::TargetAction::Knob1);
                    patch_manager->handleKnobInput(knob_idx, (float)event.value);
                    continue;
                }

                if (action >= smk::TargetAction::Pad1 && action <= smk::TargetAction::Pad16) {
                    uint8_t pad_idx = static_cast<uint8_t>(action) - static_cast<uint8_t>(smk::TargetAction::Pad1);
                    if ((event.type == smk::EventType::NoteOn && event.value > 0) || (event.type == smk::EventType::ControlChange && event.value > 64)) {
                        if (pad_idx < 8) {
                            // If currently viewing Scenes Screen -> Pads 1..8 select/cue or save scenes
                            if (ui_manager && ui_manager->activeScreenId() == smk::ScreenId::Scenes && scene_manager) {
                                if (step_sequencer && step_sequencer->isRecording()) {
                                    scene_manager->captureCurrentAsScene(pad_idx);
                                } else {
                                    scene_manager->selectScene(pad_idx, false);
                                }
                            }
                            // If currently viewing Sequencer Screen
                            else if (ui_manager && ui_manager->activeScreenId() == smk::ScreenId::Sequencer && step_sequencer) {
                                if (step_sequencer->isRecording()) {
                                    // Live Tap Record: Quantize to current step and trigger voice
                                    step_sequencer->recordLiveTrackHit(pad_idx % 4, (uint8_t)event.value);
                                    if (pad_manager) {
                                        pad_manager->handlePadPress(pad_idx, (uint8_t)event.value, amy_adapter, ui_manager);
                                    }
                                } else {
                                    // Direct Step Edit: Toggle step on/off
                                    uint8_t step_idx = step_sequencer->stepPage() * 8 + pad_idx;
                                    step_sequencer->toggleStep(step_sequencer->selectedTrack(), step_idx, (uint8_t)event.value);
                                    // Audio preview hit
                                    if (amy_adapter) {
                                        amy_adapter->noteOn(9, step_sequencer->trackNote(step_sequencer->selectedTrack()), (uint8_t)event.value);
                                    }
                                }
                            } else {
                                if (pad_manager) {
                                    pad_manager->handlePadPress(pad_idx, (uint8_t)event.value, amy_adapter, ui_manager);
                                }
                                if (step_sequencer && step_sequencer->isRecording() && pad_manager && pad_manager->activeBank() == smk::PadBank::BankA_Drums) {
                                    step_sequencer->recordLiveTrackHit(pad_idx % 4, (uint8_t)event.value);
                                }
                            }
                        } else {
                            // Pad Bank B (Pads 9..16) Contextual Navigation & Shortcuts
                            uint8_t b_idx = pad_idx - 8;
                            if (ui_manager && ui_manager->activeScreenId() == smk::ScreenId::Sequencer && step_sequencer) {
                                switch (b_idx) {
                                    case 0: // Select BD Track
                                        step_sequencer->selectTrack(0);
                                        break;
                                    case 1: // Select SD Track
                                        step_sequencer->selectTrack(1);
                                        break;
                                    case 2: // Select CH Track
                                        step_sequencer->selectTrack(2);
                                        break;
                                    case 3: // Select OH Track
                                        step_sequencer->selectTrack(3);
                                        break;
                                    case 4: // Toggle Page (1-8 <-> 9-16)
                                        step_sequencer->toggleStepPage();
                                        break;
                                    case 5: // Toggle Mute on active track
                                        step_sequencer->toggleTrackMute(step_sequencer->selectedTrack());
                                        break;
                                    case 6: // Toggle Solo on active track
                                        step_sequencer->toggleTrackSolo(step_sequencer->selectedTrack());
                                        break;
                                    case 7: // Clear active track
                                        step_sequencer->clearTrack(step_sequencer->selectedTrack());
                                        break;
                                }
                            } else {
                                switch (b_idx) {
                                    case 0: // Pad 1 (B): Previous Patch
                                        if (patch_manager) patch_manager->previousPatch();
                                        break;
                                    case 1: // Pad 2 (B): Next Patch
                                        if (patch_manager) patch_manager->nextPatch();
                                        break;
                                    case 2: // Pad 3 (B): Toggle Active Knob Bank
                                        if (patch_manager) patch_manager->nextKnobBank();
                                        break;
                                    case 3: // Pad 4 (B): Toggle Arpeggiator On/Off
                                        if (arpeggiator) {
                                            arpeggiator->setEnabled(!arpeggiator->isEnabled());
                                            if (ui_manager) {
                                                ui_manager->triggerParameterOverlay("ARPEGGIATOR", "ARP", arpeggiator->isEnabled() ? 1.0f : 0.0f, 0.0f, arpeggiator->isEnabled() ? "ON" : "OFF", smk::TakeoverStatus::Captured);
                                            }
                                        }
                                        break;
                                    case 4: // Pad 5 (B): Previous UI Page
                                        if (ui_manager) ui_manager->previousPage();
                                        break;
                                    case 5: // Pad 6 (B): Next UI Page
                                        if (ui_manager) ui_manager->nextPage();
                                        break;
                                    case 6: // Pad 7 (B): Toggle / Open MIDI Learn Screen
                                        if (ui_manager) {
                                            if (ui_manager->activeScreenId() == smk::ScreenId::MidiLearn) {
                                                ui_manager->switchScreen(smk::ScreenId::Home);
                                            } else {
                                                ui_manager->switchScreen(smk::ScreenId::MidiLearn);
                                            }
                                        }
                                        break;
                                    case 7: // Pad 8 (B): Quick Save Patch, Profile & Scenes
                                        if (storage_manager && patch_manager) {
                                            if (storage_manager->savePatch(patch_manager->activePatchId(), patch_manager->activePatch())) {
                                                storage_manager->saveProfile("smk25_custom", active_profile);
                                                if (scene_manager) scene_manager->saveAllToFlash();
                                                ESP_LOGI(TAG, "Quick Saved Patch, Profile & Scenes to Flash");
                                                if (ui_manager) ui_manager->triggerParameterOverlay("SAVED ALL", "SPIFFS", (float)patch_manager->activePatchId(), 0.0f, patch_manager->activePatch().name, smk::TakeoverStatus::Captured);
                                            }
                                        }
                                        break;
                                    default:
                                        break;
                                }
                            }
                        }
                    } else {
                        if (pad_idx < 8) {
                            if (ui_manager && ui_manager->activeScreenId() == smk::ScreenId::Sequencer && step_sequencer && !step_sequencer->isRecording()) {
                                if (amy_adapter) amy_adapter->noteOff(9, step_sequencer->trackNote(step_sequencer->selectedTrack()));
                            } else if (pad_manager) {
                                pad_manager->handlePadRelease(pad_idx, amy_adapter);
                            }
                        }
                    }
                    if (amy_adapter && ui_manager && patch_manager) {
                        uint32_t active_v = amy_adapter->activeVoices();
                        ui_manager->homeScreen().setActiveVoices(active_v, patch_manager->activePatch().voice_count);
                        smk::Diagnostics::instance().counters().active_voices.store(active_v);
                    }
                    continue;
                }

                switch (action) {
                    case smk::TargetAction::Play:
                        if (event.value > 0) {
                            if (ui_manager && ui_manager->activeScreenId() == smk::ScreenId::MidiLearn) {
                                midi_learn->begin(&active_profile);
                                midi_learn->startWizard();
                                ui_manager->midiLearnScreen().triggerFeedback("WIZARD INICIADO!", smk::DisplayDriver::kColorGreen);
                            } else {
                                if (step_sequencer->isPlaying()) step_sequencer->stop();
                                else { clock_manager->start(); step_sequencer->play(); }
                            }
                        }
                        continue;
                    case smk::TargetAction::Stop:
                        if (event.value > 0) {
                            if (midi_learn && midi_learn->isLearning()) {
                                midi_learn->skipStep();
                                if (ui_manager) ui_manager->midiLearnScreen().triggerFeedback("PASSO PULADO", smk::DisplayDriver::kColorAmber);
                            } else {
                                step_sequencer->stop(); arpeggiator->reset(); amy_adapter->allNotesOff();
                            }
                        }
                        continue;
                    case smk::TargetAction::Rec:
                        if (event.value > 0) {
                            if (midi_learn && midi_learn->isLearning()) {
                                midi_learn->cancel();
                                if (ui_manager) ui_manager->midiLearnScreen().triggerFeedback("CANCELADO", smk::DisplayDriver::kColorRed);
                            } else {
                                step_sequencer->record();
                            }
                        }
                        continue;
                    default:
                        break;
                }
            }

            switch (event.type) {
                case smk::EventType::NoteOn:
                    if (event.source == smk::EventSource::UsbMidi && arpeggiator->isEnabled()) {
                        if (clock_manager && !clock_manager->isRunning()) {
                            clock_manager->start();
                        }
                        arpeggiator->noteOn((uint8_t)event.id, (uint8_t)event.value);
                    } else {
                        amy_adapter->noteOn(event.channel, (uint8_t)event.id, (uint8_t)event.value);
                    }
                    break;
                case smk::EventType::NoteOff:
                    if (event.source == smk::EventSource::UsbMidi && arpeggiator->isEnabled()) {
                        arpeggiator->noteOff((uint8_t)event.id);
                    } else {
                        amy_adapter->noteOff(event.channel, (uint8_t)event.id);
                    }
                    break;
                case smk::EventType::PitchBend:
                    amy_adapter->pitchBend(event.channel, (int16_t)event.value);
                    break;
                case smk::EventType::Modulation:
                    patch_manager->setMacro(2, (float)event.value, true);
                    break;
                case smk::EventType::ControlChange:
                    if (event.id >= 1 && event.id <= 8) {
                        patch_manager->handleKnobInput(event.id - 1, (float)event.value);
                    } else {
                        amy_adapter->controlChange(event.channel, (uint8_t)event.id, (uint8_t)event.value);
                        if (ui_manager) {
                            char cc_title[32];
                            snprintf(cc_title, sizeof(cc_title), "CC #%d", event.id);
                            ui_manager->triggerParameterOverlay(cc_title, "SYNTH", (float)event.value, 64.0f, "", smk::TakeoverStatus::Captured);
                        }
                    }
                    break;
                case smk::EventType::ButtonPress:
                    switch (static_cast<smk::ButtonId>(event.id)) {
                        case smk::ButtonId::KnobBank:
                            patch_manager->nextKnobBank();
                            break;
                        case smk::ButtonId::PadBank:
                            if (ui_manager && ui_manager->activeScreenId() == smk::ScreenId::Sequencer && step_sequencer) {
                                step_sequencer->toggleStepPage();
                            } else if (pad_manager) {
                                pad_manager->nextBank();
                            }
                            break;
                        case smk::ButtonId::Play:
                            if (step_sequencer->isPlaying()) {
                                step_sequencer->stop();
                            } else {
                                clock_manager->start();
                                step_sequencer->play();
                            }
                            break;
                        case smk::ButtonId::Stop:
                            step_sequencer->stop();
                            arpeggiator->reset();
                            amy_adapter->allNotesOff();
                            break;
                        case smk::ButtonId::Record:
                            step_sequencer->record();
                            break;
                        case smk::ButtonId::OctavePlus:
                        case smk::ButtonId::OctaveMinus:
                            break;
                        case smk::ButtonId::Arp:
                            arpeggiator->setEnabled(!arpeggiator->isEnabled(), event_bus);
                            if (ui_manager) {
                                ui_manager->triggerParameterOverlay(
                                    "ARPEGGIATOR", 
                                    arpeggiator->isEnabled() ? "ENABLED" : "DISABLED", 
                                    arpeggiator->isEnabled() ? 127.0f : 0.0f, 
                                    0.0f, 
                                    "", 
                                    smk::TakeoverStatus::Captured
                                );
                            }
                            break;
                        case smk::ButtonId::Scene:
                            if (ui_manager) {
                                if (ui_manager->activeScreenId() == smk::ScreenId::Scenes) {
                                    if (scene_manager) scene_manager->nextScene();
                                } else {
                                    ui_manager->switchScreen(smk::ScreenId::Scenes);
                                }
                            }
                            break;
                        default:
                            break;
                    }
                    break;
                case smk::EventType::Clock:
                    if (clock_manager) clock_manager->onExternalTick();
                    break;
                case smk::EventType::TransportPlay:
                    if (step_sequencer && clock_manager) {
                        clock_manager->onExternalStart();
                        step_sequencer->play();
                    }
                    break;
                case smk::EventType::TransportStop:
                    if (step_sequencer && clock_manager) {
                        clock_manager->onExternalStop();
                        step_sequencer->stop();
                        if (arpeggiator) arpeggiator->reset();
                        if (amy_adapter) amy_adapter->allNotesOff();
                    }
                    break;
                case smk::EventType::AllNotesOff:
                    arpeggiator->reset();
                    step_sequencer->stop();
                    amy_adapter->allNotesOff();
                    break;
                case smk::EventType::Panic:
                    ESP_LOGW(TAG, "PANIC received! Silencing engine.");
                    arpeggiator->reset();
                    step_sequencer->stop();
                    amy_adapter->panic();
                    break;
                case smk::EventType::UsbConnect: {
                    uint16_t vid = event.id;
                    uint16_t pid = (uint16_t)event.value;
                    ESP_LOGI(TAG, "USB MIDI Device Connected! VID: 0x%04X, PID: 0x%04X", vid, pid);
                    smk::Diagnostics::instance().counters().usb_reconnects.fetch_add(1, std::memory_order_relaxed);
                    smk::Diagnostics::instance().counters().usb_connected.store(true, std::memory_order_relaxed);

                    char prof_filename[32];
                    snprintf(prof_filename, sizeof(prof_filename), "prof_%04X_%04X", vid, pid);

                    smk::ControllerProfile loaded_prof = {};
                    if (storage_manager && storage_manager->loadProfile(prof_filename, loaded_prof)) {
                        active_profile = loaded_prof;
                        ESP_LOGI(TAG, "Auto-loaded controller profile [%s] from Flash", prof_filename);
                    } else if (storage_manager && storage_manager->loadProfile("smk25_custom", loaded_prof)) {
                        active_profile = loaded_prof;
                        ESP_LOGI(TAG, "Auto-loaded default profile [smk25_custom] from Flash");
                    } else {
                        active_profile = smk::ProfileManager::createDefaultSmk25Profile();
                        ESP_LOGI(TAG, "Applied default SMK25 profile for VID: 0x%04X PID: 0x%04X", vid, pid);
                    }

                    if (ui_manager) {
                        ui_manager->homeScreen().setUsbConnected(true);
                        ui_manager->triggerParameterOverlay("USB CONNECTED", active_profile.name, 0.0f, 0.0f, "", smk::TakeoverStatus::Captured);
                    }
                    break;
                }
                case smk::EventType::UsbDisconnect: {
                    uint16_t vid = event.id;
                    uint16_t pid = (uint16_t)event.value;
                    ESP_LOGW(TAG, "USB MIDI Device Disconnected! (VID: 0x%04X, PID: 0x%04X)", vid, pid);
                    smk::Diagnostics::instance().counters().usb_disconnects.fetch_add(1, std::memory_order_relaxed);
                    smk::Diagnostics::instance().counters().usb_connected.store(false, std::memory_order_relaxed);
                    if (ui_manager) {
                        ui_manager->homeScreen().setUsbConnected(false);
                        ui_manager->triggerParameterOverlay("USB DISCONNECTED", "PANIC", 0.0f, 0.0f, "", smk::TakeoverStatus::Captured);
                    }
                    break;
                }
                default:
                    break;
            }

            // Sync active voice count to HomeScreen and Diagnostics on voice-changing events
            if (amy_adapter && (event.type == smk::EventType::NoteOn || 
                                event.type == smk::EventType::NoteOff || 
                                event.type == smk::EventType::AllNotesOff || 
                                event.type == smk::EventType::Panic ||
                                event.type == smk::EventType::UsbDisconnect)) {
                uint32_t active_v = amy_adapter->activeVoices();
                if (ui_manager && patch_manager) {
                    ui_manager->homeScreen().setActiveVoices(active_v, patch_manager->activePatch().voice_count);
                }
                smk::Diagnostics::instance().counters().active_voices.store(active_v);
            }
        }

        TickType_t now = xTaskGetTickCount();
        if (now - last_status_time >= status_interval) {
            last_status_time = now;
            if (ui_manager) {
                ui_manager->homeScreen().setUsbConnected(midi_host ? midi_host->isDeviceConnected() : false);
                ui_manager->homeScreen().setBpm(clock_manager ? clock_manager->bpm() : 120.0f);
            }
            if (midi_learn && midi_learn->isLearning()) {
                // Suppress periodic status log during MIDI Learn wizard
                continue;
            }
            auto snapshot = smk::Diagnostics::instance().takeSnapshot();
            ESP_LOGI(TAG, "Status: Patch=[%s], BPM=%.1f, Arp=%s, Seq=%s, StorageUsed=%zuKB, Voices=%lu",
                     patch_manager ? patch_manager->activePatch().name : "NONE",
                     clock_manager ? clock_manager->bpm() : 0.0f,
                     (arpeggiator && arpeggiator->isEnabled()) ? "ON" : "OFF",
                     (step_sequencer && step_sequencer->isPlaying()) ? "PLAY" : "STOP",
                     storage_manager ? storage_manager->usedBytes() / 1024 : 0,
                     snapshot.active_voices);
        }
    }
}

extern "C" void app_main() {
    xTaskCreatePinnedToCore(
        app_init_task,
        "app_init",
        16 * 1024,
        NULL,
        smk::config::kControlTaskPriority,
        NULL,
        0
    );
}
