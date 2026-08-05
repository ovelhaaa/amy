#include "app_config.h"
#include "diagnostics.h"
#include "console.h"

// Assuming these headers will be available for components:
#include "event_bus.h"
#include "pcm5102_output.h"
#include "amy_adapter.h"
#include "audio_task.h"
#include "usb_midi_host.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_psram.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "Main";

extern "C" void app_main() {
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

    // 3. Create EventBus
    smk::EventBus* event_bus = new smk::EventBus(smk::config::kEventQueueCapacity);

    // 4. Create Pcm5102Output
    smk::PCM5102Output* pcm_out = new smk::PCM5102Output(
        smk::config::kI2sBclk,
        smk::config::kI2sLrclk,
        smk::config::kI2sData
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


    // 9. Create UsbMidiHost
    smk::UsbMidiHost* midi_host = new smk::UsbMidiHost(*event_bus);

    // 10. Start USB MIDI Host
    if (!midi_host->begin()) {
        ESP_LOGE(TAG, "Failed to initialize USB MIDI Host");
    }

    // 11. Create Console and start it
    smk::Console* console = new smk::Console();
    if (!console->begin()) {
        ESP_LOGE(TAG, "Failed to initialize Console");
    }

    ESP_LOGI(TAG, "Initialization complete");

    // 13. Enter main control loop
    TickType_t last_status_time = xTaskGetTickCount();
    const TickType_t status_interval = pdMS_TO_TICKS(5000);

    while (true) {
        smk::SynthEvent event;
        if (event_bus->receive(event, 10)) {
            switch (event.type) {
                case smk::EventType::NoteOn:
                    amy_adapter->noteOn(event.channel, event.id, event.value);
                    break;
                case smk::EventType::NoteOff:
                    amy_adapter->noteOff(event.channel, event.id);
                    break;
                case smk::EventType::PitchBend:
                    amy_adapter->pitchBend(event.channel, event.value);
                    break;
                case smk::EventType::Modulation:
                    amy_adapter->controlChange(event.channel, 1, event.value);
                    break;
                case smk::EventType::ControlChange:
                    amy_adapter->controlChange(event.channel, event.id, event.value);
                    break;
                case smk::EventType::AllNotesOff:
                    amy_adapter->allNotesOff();
                    break;
                case smk::EventType::Panic:
                    ESP_LOGW(TAG, "PANIC received! Silencing engine.");
                    amy_adapter->panic();
                    break;
                default:
                    break;
            }
        }

        TickType_t now = xTaskGetTickCount();
        if (now - last_status_time >= status_interval) {
            last_status_time = now;
            auto snapshot = smk::Diagnostics::instance().takeSnapshot();
            ESP_LOGI(TAG, "Status: Voices=%lu, Underruns=%lu, Audio Load=%.2f",
                     snapshot.active_voices, snapshot.audio_underruns, snapshot.render_load);
        }
    }
}
