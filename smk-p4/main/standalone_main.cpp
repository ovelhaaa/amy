#include "sdkconfig.h"
#include "app_config.h"
#include "gate0_diagnostics.h"
#include "display_test.h"
#include "pcm5102_output.h"
#include "pcm_test.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <cinttypes>

#if defined(CONFIG_SMK_LINK_ENABLED)
#error "Standalone must not initialize the S3 UART bridge"
#endif

namespace {
using namespace smk::p4::audio;
constexpr const char* kTag = "P4_PCM_TEST";
struct Snapshot {
    uint64_t frames = 0, render_total_us = 0;
    uint32_t blocks = 0, render_max_us = 0, render_deadlines = 0;
    uint32_t service_max_us = 0, refill_risks = 0, write_errors = 0, timeouts = 0;
    unsigned stack_free_bytes = 0;
    esp_err_t error = ESP_OK;
    bool running = false;
};
StaticQueue_t snapshot_control;
uint8_t snapshot_storage[sizeof(Snapshot)];
QueueHandle_t snapshots = nullptr;

void runAudio() {
    Pcm5102Output output;
    TestTone tone;
    Snapshot state;
    int16_t pcm[kBlockSamples]{};
    state.error = tone.configure(CONFIG_SMKP4_PCM_TEST_HZ, kTestAmplitude) ?
        output.begin(CONFIG_SMKP4_PCM_BCLK, CONFIG_SMKP4_PCM_WS, CONFIG_SMKP4_PCM_DATA) : ESP_ERR_INVALID_ARG;
    if (state.error == ESP_OK) state.error = output.start();
    state.running = state.error == ESP_OK;
    xQueueOverwrite(snapshots, &state);
    int64_t previous_start = 0;
    while (state.running) {
        const int64_t start = esp_timer_get_time();
        if (previous_start && start - previous_start >
            static_cast<int64_t>(kDmaDescriptors * kBlockFrames * 1000000ULL / kSampleRateHz))
            ++state.refill_risks; // Scheduling risk, not a measured analog dropout.
        previous_start = start;
#if defined(CONFIG_SMKP4_PCM_TEST_SEQUENCE)
        constexpr bool sequence = true;
#else
        constexpr bool sequence = false;
#endif
        if (!tone.render(pcm, kBlockFrames, sequence)) {
            state.error = ESP_FAIL;
            break;
        }
        const uint32_t render_us = static_cast<uint32_t>(esp_timer_get_time() - start);
        state.render_total_us += render_us;
        ++state.blocks; // Includes generated blocks whose subsequent I2S write fails.
        if (render_us > state.render_max_us) state.render_max_us = render_us;
        if (render_us * static_cast<uint64_t>(kSampleRateHz) > kBlockFrames * 1000000ULL)
            ++state.render_deadlines;
        state.error = output.write(pcm, kBlockFrames);
        const uint32_t service_us = static_cast<uint32_t>(esp_timer_get_time() - start);
        if (service_us > state.service_max_us) state.service_max_us = service_us;
        if (state.error != ESP_OK) {
            ++state.write_errors;
            if (state.error == ESP_ERR_TIMEOUT) ++state.timeouts;
            break; // Stop clocks; never loop rapidly on a failed DMA channel.
        }
        state.frames += kBlockFrames;
        state.stack_free_bytes = uxTaskGetStackHighWaterMark(nullptr);
        xQueueOverwrite(snapshots, &state); // Fixed single-element mailbox, no waiting.
    }
    const esp_err_t stop_error = output.stop();
    if (state.error == ESP_OK) state.error = stop_error;
    state.running = false;
    xQueueOverwrite(snapshots, &state);
}
void audioTask(void*) {
    runAudio(); // RAII releases the driver before deleting the FreeRTOS task.
    vTaskDelete(nullptr);
}
}

extern "C" void app_main() {
    if (!smk::p4::reportHardware()) return;
    ESP_LOGI(kTag, "Standalone Gate A only; no AMY, UART, USB Host, ES8311 or NS4150");
    ESP_LOGI(kTag, "PCM=%" PRIu32 "Hz signed16 stereo Philips; block=%u DMA=%ux%u MCLK=unused",
             kSampleRateHz, unsigned(kBlockFrames), kDmaDescriptors, unsigned(kBlockFrames));
    snapshots = xQueueCreateStatic(1, sizeof(Snapshot), snapshot_storage, &snapshot_control);
#if defined(CONFIG_SMKP4_PCM_WIRING_CONFIRMED)
    ESP_LOGI(kTag, "I2S BCLK=%d WS=%d DATA=%d test_sequence=%d frequency=%dHz peak=%.2f",
             CONFIG_SMKP4_PCM_BCLK, CONFIG_SMKP4_PCM_WS, CONFIG_SMKP4_PCM_DATA,
#if defined(CONFIG_SMKP4_PCM_TEST_SEQUENCE)
             1,
#else
             0,
#endif
             CONFIG_SMKP4_PCM_TEST_HZ, double(kTestAmplitude));
    if (!snapshots || xTaskCreate(audioTask, "pcm_test", kTaskStackBytes, nullptr,
                                  kTaskPriority, nullptr) != pdPASS) {
        ESP_LOGE(kTag, "AUDIO FAIL: task/mailbox creation failed");
    }
#else
    // Keep the driver task type-checked even when electrical approval is absent.
    (void)audioTask;
    ESP_LOGW(kTag, "AUDIO BLOCKED: confirm wiring in menuconfig; GPIOs not driven");
#endif
    const bool display_ready = smk::p4::beginDisplayTest();
    if (!display_ready) ESP_LOGE(kTag, "Display failed; independent audio test continues");
    TickType_t last_report = xTaskGetTickCount();
    for (;;) {
        if (display_ready) smk::p4::serviceDisplayTest();
        if (xTaskGetTickCount() - last_report >= pdMS_TO_TICKS(smk::p4::config::kDiagnosticsIntervalMs)) {
            last_report = xTaskGetTickCount();
            Snapshot state;
            if (xQueuePeek(snapshots, &state, 0) == pdTRUE) {
                ESP_LOGI(kTag, "AUDIO %s frames_written=%" PRIu64 " blocks_rendered=%" PRIu32
                         " render_avg_us=%" PRIu64 " render_max_us=%" PRIu32
                         " render_deadlines=%" PRIu32 " service_max_us=%" PRIu32
                         " refill_risks=%" PRIu32 " write_errors=%" PRIu32 " timeouts=%" PRIu32
                         " stack_free_bytes=%u error=%s DMA_underruns=UNMEASURED",
                         state.running ? "PRODUCING_TEST_PCM" : "FAIL", state.frames, state.blocks,
                         state.blocks ? state.render_total_us / state.blocks : 0, state.render_max_us,
                         state.render_deadlines, state.service_max_us, state.refill_risks,
                         state.write_errors, state.timeouts, state.stack_free_bytes, esp_err_to_name(state.error));
            }
            smk::p4::reportMemory();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
