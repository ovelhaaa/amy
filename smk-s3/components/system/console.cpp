#include "console.h"
#include "diagnostics.h"
#include "ui_manager.h"
#include "patch_manager.h"
#include "factory_patches.h"
#include "clock_manager.h"
#include "arpeggiator.h"
#include "step_sequencer.h"
#include "storage_manager.h"
#include "nvs_storage.h"
#include "controller_profile.h"
#include "midi_learn.h"
#include "pad_bank.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"
#include <cstring>
#include <cstdlib>

static const char* TAG = "Console";

namespace smk {

UIManager*      Console::s_ui_manager      = nullptr;
PatchManager*   Console::s_patch_manager   = nullptr;
ClockManager*   Console::s_clock_manager   = nullptr;
Arpeggiator*    Console::s_arpeggiator     = nullptr;
StepSequencer*  Console::s_sequencer       = nullptr;
StorageManager* Console::s_storage_manager = nullptr;
NvsStorage*     Console::s_nvs_storage     = nullptr;
MidiLearn*      Console::s_midi_learn      = nullptr;
PadManager*     Console::s_pad_manager     = nullptr;

static ControllerProfile s_active_profile = ProfileManager::createDefaultSmk25Profile();

Console::Console() {}

void Console::setUiManager(UIManager* ui_mgr) { s_ui_manager = ui_mgr; }
void Console::setPatchManager(PatchManager* patch_mgr) { s_patch_manager = patch_mgr; }
void Console::setClockManager(ClockManager* clock_mgr) { s_clock_manager = clock_mgr; }
void Console::setArpeggiator(Arpeggiator* arp) { s_arpeggiator = arp; }
void Console::setStepSequencer(StepSequencer* seq) { s_sequencer = seq; }
void Console::setStorageManager(StorageManager* storage_mgr) { s_storage_manager = storage_mgr; }
void Console::setNvsStorage(NvsStorage* nvs_storage) { s_nvs_storage = nvs_storage; }
void Console::setMidiLearn(MidiLearn* midi_learn) { s_midi_learn = midi_learn; }
void Console::setPadManager(PadManager* pad_mgr) { s_pad_manager = pad_mgr; }

bool Console::begin() {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT
    };
    
    uart_param_config(UART_NUM_0, &uart_config);
    uart_driver_install(UART_NUM_0, 1024, 0, 0, NULL, 0);

    esp_console_config_t console_config = {};
    console_config.max_cmdline_args = 8;
    console_config.max_cmdline_length = 256;
    console_config.hint_color = 39;
    
    if (esp_console_init(&console_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize console");
        return false;
    }

    registerCommand("status", "Show system status snapshot", cmdStatus);
    registerCommand("audio_status", "Show audio subsystem status", cmdAudioStatus);
    registerCommand("panic", "Trigger engine panic (all notes off)", cmdPanic);
    registerCommand("memory", "Show detailed memory report", cmdMemory);
    registerCommand("midi_monitor", "Toggle MIDI monitor", cmdMidiMonitor);
    registerCommand("reboot", "Reboot the device", cmdReboot);
    registerCommand("ui_screen", "Switch UI screen (home|system|midi|seq|pads)", cmdUiScreen);
    registerCommand("ui_param", "Trigger UI parameter popup <name> <val>", cmdUiParam);
    registerCommand("patch_list", "List embedded factory patches", cmdPatchList);
    registerCommand("patch_select", "Select active patch <id>", cmdPatchSelect);
    registerCommand("macro_set", "Set macro value <id 0..7> <val 0..127>", cmdMacroSet);
    registerCommand("bpm_set", "Set global BPM <30..300>", cmdBpmSet);
    registerCommand("arp_enable", "Enable/disable arpeggiator <0|1>", cmdArpEnable);
    registerCommand("arp_mode", "Set arpeggiator mode <up|down|updown|random>", cmdArpMode);
    registerCommand("seq_step", "Set sequencer step <step 0..15> <note> <vel> <active>", cmdSeqStep);
    registerCommand("seq_play", "Start step sequencer", cmdSeqPlay);
    registerCommand("seq_stop", "Start step sequencer", cmdSeqStop);
    registerCommand("storage_info", "Show SPIFFS Flash storage status", cmdStorageInfo);
    registerCommand("patch_save", "Save active patch to Flash slot <slot 0..127>", cmdPatchSave);
    registerCommand("patch_load", "Load patch from Flash slot <slot 0..127>", cmdPatchLoad);
    registerCommand("profile_save", "Save controller profile <name>", cmdProfileSave);
    registerCommand("profile_load", "Load controller profile <name>", cmdProfileLoad);
    registerCommand("profile_show", "Show active controller profile bindings", cmdProfileShow);
    registerCommand("learn_start", "Start MIDI Learn wizard", cmdLearnStart);
    registerCommand("learn_cancel", "Cancel MIDI Learn wizard", cmdLearnCancel);
    registerCommand("knob_bank", "Switch knob bank <a|b|c|d|e>", cmdKnobBank);
    registerCommand("pad_bank", "Switch pad bank <a|b|c|d>", cmdPadBank);
    registerCommand("scene_save", "Save live scene <name>", cmdSceneSave);
    registerCommand("scene_load", "Load live scene <name>", cmdSceneLoad);
    registerCommand("sys_save", "Save current system state to NVS", cmdSysSave);
    registerCommand("sys_load", "Load system state from NVS", cmdSysLoad);
    registerCommand("help", "List available commands", cmdHelp);

    xTaskCreatePinnedToCore(consoleTask, "console_task", 8192, this, 2, &task_handle_, 0);
    
    return true;
}

void Console::registerCommand(const char* name, const char* help, esp_console_cmd_func_t func) {
    esp_console_cmd_t cmd = {};
    cmd.command = name;
    cmd.help = help;
    cmd.func = func;
    esp_console_cmd_register(&cmd);
}

int Console::cmdStatus(int argc, char** argv) {
    Diagnostics::instance().logSnapshot();
    return 0;
}

int Console::cmdAudioStatus(int argc, char** argv) {
    auto& counters = Diagnostics::instance().counters();
    ESP_LOGI(TAG, "Audio Underruns: %lu", counters.audio_underruns.load());
    ESP_LOGI(TAG, "Max Render Us: %lu", counters.max_render_us.load());
    ESP_LOGI(TAG, "Avg Render Us: %lu", counters.avg_render_us.load());
    ESP_LOGI(TAG, "Frames Rendered: %lu", counters.frames_rendered.load());
    return 0;
}

int Console::cmdPanic(int argc, char** argv) {
    ESP_LOGW(TAG, "Panic triggered!");
    return 0;
}

int Console::cmdMemory(int argc, char** argv) {
    ESP_LOGI(TAG, "Internal RAM Free: %zu", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    ESP_LOGI(TAG, "PSRAM Free: %zu", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    return 0;
}

int Console::cmdMidiMonitor(int argc, char** argv) {
    ESP_LOGI(TAG, "MIDI monitor toggled.");
    return 0;
}

int Console::cmdReboot(int argc, char** argv) {
    ESP_LOGW(TAG, "Rebooting...");
    esp_restart();
    return 0;
}

int Console::cmdUiScreen(int argc, char** argv) {
    if (!s_ui_manager) return 1;
    if (argc < 2) return 1;

    if (strcmp(argv[1], "home") == 0) s_ui_manager->switchScreen(ScreenId::Home);
    else if (strcmp(argv[1], "system") == 0) s_ui_manager->switchScreen(ScreenId::System);
    else if (strcmp(argv[1], "midi") == 0) s_ui_manager->switchScreen(ScreenId::MidiMonitor);
    else if (strcmp(argv[1], "seq") == 0) s_ui_manager->switchScreen(ScreenId::Sequencer);
    else if (strcmp(argv[1], "pads") == 0) s_ui_manager->switchScreen(ScreenId::Pads);

    return 0;
}

int Console::cmdUiParam(int argc, char** argv) {
    if (!s_ui_manager) return 1;
    const char* param_name = (argc >= 2) ? argv[1] : "CUTOFF FREQ";
    float val = (argc >= 3) ? (float)atof(argv[2]) : 85.0f;

    s_ui_manager->triggerParameterOverlay(param_name, "LAYER A", val, 64.0f, "", TakeoverStatus::Captured);
    return 0;
}

int Console::cmdPatchList(int argc, char** argv) {
    ESP_LOGI(TAG, "--- Factory Patches ---");
    size_t count = FactoryPatches::count();
    for (size_t i = 0; i < count; ++i) {
        const SynthPatch* p = FactoryPatches::getPatchByIndex(i);
        if (p != nullptr) {
            ESP_LOGI(TAG, "[%03d] %-20s (Voices: %d, Mode: %d)", (int)p->id, p->name, (int)p->voice_count, (int)p->mode);
        }
    }
    return 0;
}

int Console::cmdPatchSelect(int argc, char** argv) {
    if (!s_patch_manager) return 1;
    if (argc < 2) return 1;
    uint8_t patch_id = (uint8_t)atoi(argv[1]);
    if (s_patch_manager->selectPatch(patch_id)) {
        ESP_LOGI(TAG, "Loaded Patch #%d [%s]", patch_id, s_patch_manager->activePatch().name);
    }
    return 0;
}

int Console::cmdMacroSet(int argc, char** argv) {
    if (!s_patch_manager) return 1;
    if (argc < 3) return 1;
    uint8_t macro_id = (uint8_t)atoi(argv[1]);
    float val = (float)atof(argv[2]);
    s_patch_manager->setMacro(macro_id, val, true);
    ESP_LOGI(TAG, "Set Macro #%d = %.1f", macro_id, val);
    return 0;
}

int Console::cmdBpmSet(int argc, char** argv) {
    if (!s_clock_manager) return 1;
    if (argc < 2) return 1;
    float bpm = (float)atof(argv[1]);
    s_clock_manager->setBpm(bpm);
    ESP_LOGI(TAG, "BPM set to %.1f", s_clock_manager->bpm());
    return 0;
}

int Console::cmdArpEnable(int argc, char** argv) {
    if (!s_arpeggiator) return 1;
    if (argc < 2) return 1;
    bool enable = (atoi(argv[1]) != 0);
    s_arpeggiator->setEnabled(enable);
    ESP_LOGI(TAG, "Arpeggiator %s", enable ? "ENABLED" : "DISABLED");
    return 0;
}

int Console::cmdArpMode(int argc, char** argv) {
    if (!s_arpeggiator) return 1;
    if (argc < 2) return 1;
    if (strcmp(argv[1], "up") == 0) s_arpeggiator->setMode(ArpMode::Up);
    else if (strcmp(argv[1], "down") == 0) s_arpeggiator->setMode(ArpMode::Down);
    else if (strcmp(argv[1], "updown") == 0) s_arpeggiator->setMode(ArpMode::UpDown);
    else if (strcmp(argv[1], "random") == 0) s_arpeggiator->setMode(ArpMode::Random);
    ESP_LOGI(TAG, "Arp Mode set to %s", argv[1]);
    return 0;
}

int Console::cmdSeqStep(int argc, char** argv) {
    if (!s_sequencer) return 1;
    if (argc < 5) return 1;
    uint8_t step = (uint8_t)atoi(argv[1]);
    uint8_t note = (uint8_t)atoi(argv[2]);
    uint8_t vel  = (uint8_t)atoi(argv[3]);
    bool active  = (atoi(argv[4]) != 0);

    s_sequencer->setStep(step, note, vel, active);
    ESP_LOGI(TAG, "Seq Step #%d -> Note:%d Vel:%d Active:%d", step, note, vel, active);
    return 0;
}

int Console::cmdSeqPlay(int argc, char** argv) {
    if (!s_sequencer || !s_clock_manager) return 1;
    s_clock_manager->start();
    s_sequencer->play();
    ESP_LOGI(TAG, "Sequencer PLAY");
    return 0;
}

int Console::cmdSeqStop(int argc, char** argv) {
    if (!s_sequencer) return 1;
    s_sequencer->stop();
    ESP_LOGI(TAG, "Sequencer STOP");
    return 0;
}

int Console::cmdStorageInfo(int argc, char** argv) {
    if (!s_storage_manager) return 1;
    ESP_LOGI(TAG, "SPIFFS Partition: Mounted=%d, Total=%zu KB, Used=%zu KB, Free=%zu KB",
             s_storage_manager->isMounted(),
             s_storage_manager->totalBytes() / 1024,
             s_storage_manager->usedBytes() / 1024,
             (s_storage_manager->totalBytes() - s_storage_manager->usedBytes()) / 1024);
    return 0;
}

int Console::cmdPatchSave(int argc, char** argv) {
    if (!s_storage_manager || !s_patch_manager) return 1;
    if (argc < 2) return 1;
    uint8_t slot_id = (uint8_t)atoi(argv[1]);
    if (s_storage_manager->savePatch(slot_id, s_patch_manager->activePatch())) {
        ESP_LOGI(TAG, "Successfully saved Patch to Flash Slot #%d", slot_id);
    } else {
        ESP_LOGE(TAG, "Failed to save Patch to Flash Slot #%d", slot_id);
    }
    return 0;
}

int Console::cmdPatchLoad(int argc, char** argv) {
    if (!s_storage_manager || !s_patch_manager) return 1;
    if (argc < 2) return 1;
    uint8_t slot_id = (uint8_t)atoi(argv[1]);
    SynthPatch loaded_patch = {};
    if (s_storage_manager->loadPatch(slot_id, loaded_patch)) {
        s_patch_manager->selectPatch(slot_id);
        ESP_LOGI(TAG, "Successfully loaded Patch #%d [%s] from Flash", slot_id, loaded_patch.name);
    } else {
        ESP_LOGE(TAG, "Failed to load Patch from Flash Slot #%d", slot_id);
    }
    return 0;
}

int Console::cmdProfileSave(int argc, char** argv) {
    if (!s_storage_manager) return 1;
    const char* profile_name = (argc >= 2) ? argv[1] : "smk25_custom";
    if (s_storage_manager->saveProfile(profile_name, s_active_profile)) {
        ESP_LOGI(TAG, "Saved profile [%s] (.s3m) to Flash", profile_name);
    } else {
        ESP_LOGE(TAG, "Failed to save profile [%s]", profile_name);
    }
    return 0;
}

int Console::cmdProfileLoad(int argc, char** argv) {
    if (!s_storage_manager) return 1;
    const char* profile_name = (argc >= 2) ? argv[1] : "smk25_custom";
    ControllerProfile loaded = {};
    if (s_storage_manager->loadProfile(profile_name, loaded)) {
        s_active_profile = loaded;
        ESP_LOGI(TAG, "Loaded profile [%s] (.s3m) from Flash", profile_name);
    } else {
        ESP_LOGE(TAG, "Failed to load profile [%s]", profile_name);
    }
    return 0;
}

int Console::cmdProfileShow(int argc, char** argv) {
    ESP_LOGI(TAG, "--- Active Controller Profile: %s ---", s_active_profile.name);
    ESP_LOGI(TAG, "  Modulation: CC #%d", s_active_profile.modulation.number);
    for (uint8_t i = 0; i < 8; ++i) {
        ESP_LOGI(TAG, "  Macro #%d: CC #%d", i + 1, s_active_profile.knobs[i].number);
    }
    for (uint8_t i = 0; i < 8; ++i) {
        ESP_LOGI(TAG, "  Pad #%d: Note #%d", i + 1, s_active_profile.pads[i].number);
    }
    return 0;
}

int Console::cmdLearnStart(int argc, char** argv) {
    if (!s_midi_learn) return 1;
    s_midi_learn->begin(&s_active_profile);
    s_midi_learn->startWizard();
    ESP_LOGI(TAG, "MIDI Learn Started: %s", s_midi_learn->currentStepName());
    return 0;
}

int Console::cmdLearnCancel(int argc, char** argv) {
    if (!s_midi_learn) return 1;
    s_midi_learn->cancel();
    ESP_LOGI(TAG, "MIDI Learn Cancelled");
    return 0;
}

int Console::cmdKnobBank(int argc, char** argv) {
    if (!s_patch_manager) return 1;
    if (argc < 2) {
        s_patch_manager->nextKnobBank();
        return 0;
    }
    const char* b = argv[1];
    if (strcmp(b, "a") == 0 || strcmp(b, "macros") == 0) s_patch_manager->setKnobBank(KnobBank::BankA_Macros);
    else if (strcmp(b, "b") == 0 || strcmp(b, "osc") == 0) s_patch_manager->setKnobBank(KnobBank::BankB_Oscillator);
    else if (strcmp(b, "c") == 0 || strcmp(b, "flt") == 0) s_patch_manager->setKnobBank(KnobBank::BankC_FilterEnv);
    else if (strcmp(b, "d") == 0 || strcmp(b, "fx") == 0) s_patch_manager->setKnobBank(KnobBank::BankD_Effects);
    else if (strcmp(b, "e") == 0 || strcmp(b, "seq") == 0) s_patch_manager->setKnobBank(KnobBank::BankE_Sequencer);
    else s_patch_manager->nextKnobBank();
    return 0;
}

int Console::cmdPadBank(int argc, char** argv) {
    if (!s_pad_manager) return 1;
    if (argc < 2) {
        s_pad_manager->nextBank();
        return 0;
    }
    const char* b = argv[1];
    if (strcmp(b, "a") == 0 || strcmp(b, "drums") == 0) s_pad_manager->setBank(PadBank::BankA_Drums);
    else if (strcmp(b, "b") == 0 || strcmp(b, "melodic") == 0) s_pad_manager->setBank(PadBank::BankB_Melodic);
    else if (strcmp(b, "c") == 0 || strcmp(b, "chords") == 0) s_pad_manager->setBank(PadBank::BankC_Chords);
    else if (strcmp(b, "d") == 0 || strcmp(b, "fx") == 0) s_pad_manager->setBank(PadBank::BankD_Performance);
    else s_pad_manager->nextBank();
    return 0;
}

int Console::cmdSceneSave(int argc, char** argv) {
    if (!s_storage_manager || !s_patch_manager || !s_clock_manager) return 1;
    const char* scene_name = (argc >= 2) ? argv[1] : "live_scene_1";

    Scene scene = {};
    strncpy(scene.name, scene_name, sizeof(scene.name) - 1);
    scene.patch_id = s_patch_manager->activePatchId();
    scene.bpm = s_clock_manager->bpm();
    scene.knob_bank = static_cast<uint8_t>(s_patch_manager->activeKnobBank());
    scene.pad_bank = s_pad_manager ? static_cast<uint8_t>(s_pad_manager->activeBank()) : 0;

    for (uint8_t i = 0; i < 8; ++i) {
        scene.macro_values[i] = s_patch_manager->activePatch().macros[i].current_val;
    }
    scene.arp_enabled = s_arpeggiator ? s_arpeggiator->isEnabled() : false;
    scene.arp_mode = s_arpeggiator ? static_cast<uint8_t>(s_arpeggiator->mode()) : 0;
    scene.arp_division = s_arpeggiator ? static_cast<uint8_t>(s_arpeggiator->division()) : 0;
    scene.arp_octaves = s_arpeggiator ? s_arpeggiator->octaves() : 1;
    scene.arp_latch = s_arpeggiator ? s_arpeggiator->latch() : false;
    scene.seq_playing = s_sequencer ? s_sequencer->isPlaying() : false;

    if (s_storage_manager->saveScene(scene_name, scene)) {
        ESP_LOGI(TAG, "Successfully saved Scene [%s] (.s3s) to Flash", scene_name);
    }
    return 0;
}

int Console::cmdSceneLoad(int argc, char** argv) {
    if (!s_storage_manager || !s_patch_manager || !s_clock_manager) return 1;
    const char* scene_name = (argc >= 2) ? argv[1] : "live_scene_1";

    Scene scene = {};
    if (s_storage_manager->loadScene(scene_name, scene)) {
        s_patch_manager->selectPatch(scene.patch_id);
        s_clock_manager->setBpm(scene.bpm);
        s_patch_manager->setKnobBank(static_cast<KnobBank>(scene.knob_bank));
        if (s_pad_manager) s_pad_manager->setBank(static_cast<PadBank>(scene.pad_bank));
        if (s_arpeggiator) {
            s_arpeggiator->setEnabled(scene.arp_enabled);
            s_arpeggiator->setMode(static_cast<ArpMode>(scene.arp_mode));
            s_arpeggiator->setDivision(static_cast<ArpDivision>(scene.arp_division));
            s_arpeggiator->setOctaves(scene.arp_octaves);
            s_arpeggiator->setLatch(scene.arp_latch);
        }
        ESP_LOGI(TAG, "Successfully loaded Scene [%s] (.s3s) from Flash", scene_name);
    }
    return 0;
}

int Console::cmdSysSave(int argc, char** argv) {
    if (!s_nvs_storage || !s_patch_manager || !s_clock_manager) return 1;
    SystemConfig cfg = {};
    cfg.active_patch_id = s_patch_manager->activePatchId();
    cfg.global_bpm = s_clock_manager->bpm();
    cfg.master_volume = 100;
    cfg.midi_channel = 0;

    if (s_nvs_storage->saveConfig(cfg)) {
        ESP_LOGI(TAG, "System Config Saved to NVS");
    } else {
        ESP_LOGE(TAG, "Failed to save NVS config");
    }
    return 0;
}

int Console::cmdSysLoad(int argc, char** argv) {
    if (!s_nvs_storage || !s_patch_manager || !s_clock_manager) return 1;
    SystemConfig cfg = {};
    if (s_nvs_storage->loadConfig(cfg)) {
        s_patch_manager->selectPatch(cfg.active_patch_id);
        s_clock_manager->setBpm(cfg.global_bpm);
        ESP_LOGI(TAG, "System Config Loaded from NVS (PatchID:%d, BPM:%.1f)", cfg.active_patch_id, cfg.global_bpm);
    } else {
        ESP_LOGW(TAG, "No NVS Config found");
    }
    return 0;
}

int Console::cmdHelp(int argc, char** argv) {
    ESP_LOGI(TAG, "--- Available Commands ---");
    ESP_LOGI(TAG, " status        - Show system status snapshot");
    ESP_LOGI(TAG, " audio_status  - Show audio subsystem status");
    ESP_LOGI(TAG, " panic         - Trigger engine panic");
    ESP_LOGI(TAG, " memory        - Show detailed memory report");
    ESP_LOGI(TAG, " patch_list    - List embedded factory patches");
    ESP_LOGI(TAG, " patch_select  - Select active patch <id>");
    ESP_LOGI(TAG, " macro_set     - Set macro value <id 0..7> <val 0..127>");
    ESP_LOGI(TAG, " bpm_set       - Set BPM <30..300>");
    ESP_LOGI(TAG, " arp_enable    - Enable/disable arpeggiator <0|1>");
    ESP_LOGI(TAG, " arp_mode      - Set arpeggiator mode <up|down|updown|random>");
    ESP_LOGI(TAG, " seq_step      - Set sequencer step <0..15> <note> <vel> <active>");
    ESP_LOGI(TAG, " seq_play      - Play step sequencer");
    ESP_LOGI(TAG, " seq_stop      - Stop step sequencer");
    ESP_LOGI(TAG, " storage_info  - Show SPIFFS Flash storage status");
    ESP_LOGI(TAG, " patch_save    - Save active patch to Flash slot <0..127>");
    ESP_LOGI(TAG, " patch_load    - Load patch from Flash slot <0..127>");
    ESP_LOGI(TAG, " profile_save  - Save controller profile <name>");
    ESP_LOGI(TAG, " profile_load  - Load controller profile <name>");
    ESP_LOGI(TAG, " profile_show  - Show active controller profile bindings");
    ESP_LOGI(TAG, " learn_start   - Start MIDI Learn wizard");
    ESP_LOGI(TAG, " learn_cancel  - Cancel MIDI Learn wizard");
    ESP_LOGI(TAG, " knob_bank     - Switch knob bank <a|b|c|d|e>");
    ESP_LOGI(TAG, " pad_bank      - Switch pad bank <a|b|c|d>");
    ESP_LOGI(TAG, " scene_save    - Save live scene <name>");
    ESP_LOGI(TAG, " scene_load    - Load live scene <name>");
    ESP_LOGI(TAG, " sys_save      - Save system settings to NVS");
    ESP_LOGI(TAG, " sys_load      - Load system settings from NVS");
    ESP_LOGI(TAG, " ui_screen     - Switch UI screen (home|system|midi|seq|pads)");
    ESP_LOGI(TAG, " reboot        - Reboot the device");
    return 0;
}

void Console::consoleTask(void* arg) {
    char line_buf[256];
    size_t line_pos = 0;

    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG, "SMK-S3 Console Ready. Type 'help' for commands.");

    while (true) {
        uint8_t ch;
        int rx_bytes = uart_read_bytes(UART_NUM_0, &ch, 1, pdMS_TO_TICKS(50));
        if (rx_bytes > 0) {
            if (ch == '\r' || ch == '\n') {
                if (line_pos > 0) {
                    line_buf[line_pos] = '\0';
                    int ret;
                    esp_err_t err = esp_console_run(line_buf, &ret);
                    if (err == ESP_ERR_NOT_FOUND) {
                        ESP_LOGW(TAG, "Unrecognized command: '%s'. Type 'help' for available commands.", line_buf);
                    } else if (err == ESP_OK && ret != ESP_OK) {
                        ESP_LOGE(TAG, "Command returned error code: 0x%x (%s)", ret, esp_err_to_name(ret));
                    } else if (err != ESP_OK && err != ESP_ERR_INVALID_ARG) {
                        ESP_LOGE(TAG, "Console error: %s", esp_err_to_name(ret));
                    }
                    line_pos = 0;
                }
            } else if (ch == '\b' || ch == 127) {
                if (line_pos > 0) {
                    line_pos--;
                }
            } else if (ch >= 32 && ch <= 126) {
                if (line_pos < sizeof(line_buf) - 1) {
                    line_buf[line_pos++] = (char)ch;
                }
            }
        }
    }
}

} // namespace smk
