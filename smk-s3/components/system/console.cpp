#include "console.h"
#include "diagnostics.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_fat.h"
#include "driver/uart.h"
#include "linenoise/linenoise.h"
#include <cstring>

static const char* TAG = "Console";

namespace smk {

Console::Console() {}

bool Console::begin() {
    fflush(stdout);
    fsync(fileno(stdout));
    setvbuf(stdin, NULL, _IONBF, 0);

    esp_console_config_t console_config = {};
    console_config.max_cmdline_args = 8;
    console_config.max_cmdline_length = 256;
    console_config.hint_color = 39; // Default color
    
    if (esp_console_init(&console_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize console");
        return false;
    }

    esp_console_register_help_command();
    
    registerCommand("status", "Show system status snapshot", cmdStatus);
    registerCommand("audio_status", "Show audio subsystem status", cmdAudioStatus);
    registerCommand("panic", "Trigger engine panic (all notes off)", cmdPanic);
    registerCommand("memory", "Show detailed memory report", cmdMemory);
    registerCommand("midi_monitor", "Toggle MIDI monitor", cmdMidiMonitor);
    registerCommand("reboot", "Reboot the device", cmdReboot);
    
    linenoiseSetMultiLine(1);
    linenoiseSetCompletionCallback(&esp_console_get_completion);
    linenoiseSetHintsCallback((linenoiseHintsCallback*) &esp_console_get_hint);
    linenoiseHistorySetMaxLen(100);

    xTaskCreatePinnedToCore(consoleTask, "console_task", 4096, this, 5, &task_handle_, 0);
    
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
    printf("Audio Underruns: %lu\n", counters.audio_underruns.load());
    printf("Max Render Us: %lu\n", counters.max_render_us.load());
    printf("Avg Render Us: %lu\n", counters.avg_render_us.load());
    printf("Frames Rendered: %lu\n", counters.frames_rendered.load());
    return 0;
}

int Console::cmdPanic(int argc, char** argv) {
    printf("Panic triggered!\n");
    // This needs a callback to the engine. We can log the event or trigger an event bus message.
    return 0;
}

int Console::cmdMemory(int argc, char** argv) {
    printf("Internal RAM Free: %zu\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    printf("PSRAM Free: %zu\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    return 0;
}

int Console::cmdMidiMonitor(int argc, char** argv) {
    printf("MIDI monitor toggled.\n");
    return 0;
}

int Console::cmdReboot(int argc, char** argv) {
    printf("Rebooting...\n");
    esp_restart();
    return 0;
}

int Console::cmdHelp(int argc, char** argv) {
    return 0;
}

void Console::consoleTask(void* arg) {
    const char* prompt = "smk> ";
    while (true) {
        char* line = linenoise(prompt);
        if (line == NULL) {
            continue;
        }
        linenoiseHistoryAdd(line);
        
        int ret;
        esp_err_t err = esp_console_run(line, &ret);
        if (err == ESP_ERR_NOT_FOUND) {
            printf("Unrecognized command\n");
        } else if (err == ESP_ERR_INVALID_ARG) {
            // empty line
        } else if (err == ESP_OK && ret != ESP_OK) {
            printf("Command returned non-zero error code: 0x%x (%s)\n", ret, esp_err_to_name(ret));
        } else if (err != ESP_OK) {
            printf("Internal error: %s\n", esp_err_to_name(err));
        }
        linenoiseFree(line);
    }
}

} // namespace smk
