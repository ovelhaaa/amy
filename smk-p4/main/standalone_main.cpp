#include "sdkconfig.h"
#include "app_config.h"
#include "gate0_diagnostics.h"
#include "display_test.h"
#include "pcm5102_output.h"
#include "pcm_test.h"
#include "amy_runtime.h"
#include "self_test.h"
#include "local_performance_backend.h"
#if defined(CONFIG_SMKP4_UI_BLUEPRINT)
#include "ui_blueprint.h"
#include "ui_blueprint_performance_adapter.h"
#else
#include "performance_ui.h"
#endif
#include "usb_midi_host.h"
#include "standalone_boot.h"
#include "esp_heap_caps.h"
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
constexpr const char* kTag = "P4_AMY";
smk::p4::amy::Runtime runtime;
smk::p4::usb::Host usb_host(runtime.usbInput());
struct Snapshot {
    smk::p4::amy::Snapshot engine;
    uint64_t frames = 0, render_total_us = 0, command_total_us = 0, dma_total_us = 0;
    uint32_t command_max_us = 0, dma_max_us = 0, patch_max_us = 0, service_deadlines = 0;
    bool physical = false;
    uint32_t blocks = 0, render_max_us = 0, render_deadlines = 0;
    uint32_t service_max_us = 0, refill_risks = 0, write_errors = 0, timeouts = 0;
    unsigned stack_free_bytes = 0;
    esp_err_t error = ESP_OK;
    bool running = false;
    bool owner_initialized = false;
};
StaticQueue_t snapshot_control;
uint8_t snapshot_storage[sizeof(Snapshot)];
QueueHandle_t snapshots = nullptr;

void runAudio() {
    Pcm5102Output output;
    TestTone tone;
    Snapshot state{};
    int16_t pcm[kBlockSamples]{};
#if !defined(CONFIG_SMKP4_PCM_DIAGNOSTIC)
#if defined(CONFIG_SMKP4_AMY_RAM_SELF_TEST)
    const size_t internal_before=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
#endif
    // Boot admission before LVGL starts allocating. Upstream amy_start is void
    // and still contains unchecked boot allocations; fail before entering it
    // when the conservative internal reserve is absent.
    if(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<smk::p4::amy::kBootLargestInternalBytes ||
       !runtime.begin()) state.error=ESP_ERR_NO_MEM;
#if defined(CONFIG_SMKP4_AMY_RAM_SELF_TEST)
    smk::p4::amy::PcmMetrics metrics[smk::p4::amy::kPatchCount];
    bool passed=state.error==ESP_OK && smk::p4::amy::selfTest(runtime,metrics,[] { vTaskDelay(1); },
        []() -> uint32_t { return static_cast<uint32_t>(esp_timer_get_time()); });
    runtime.end();
    const size_t internal_after=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    const bool integrity=heap_caps_check_integrity_all(true);
    // Measure before any float-formatted owner log (newlib can allocate
    // task-local printf state). A second lifecycle distinguishes one-time
    // platform initialization from a repeated engine leak.
    const bool second_begin=passed && integrity && runtime.begin();
    if(second_begin) runtime.render();
    runtime.end();
    const size_t internal_second=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    ESP_LOGI(kTag,"RAM_SELF_TEST heap_before=%u heap_after_end=%u heap_after_second=%u integrity=%d",
             unsigned(internal_before),unsigned(internal_after),unsigned(internal_second),integrity);
    for(size_t p=0;p<smk::p4::amy::kPatchCount;++p)
        ESP_LOGI(kTag,"RAM_SELF_TEST patch=%u peak=%lu rms=%.4f clipped=%lu release=%d panic=%d dsp_avg_us=%lu dsp_max_us=%lu",
            smk::p4::amy::kPatches[p].id,(unsigned long)metrics[p].peak,metrics[p].rms,
            (unsigned long)metrics[p].clipped,metrics[p].release,metrics[p].panic,
            (unsigned long)metrics[p].dsp_avg_us,(unsigned long)metrics[p].dsp_max_us);
    passed=passed && integrity && second_begin && runtime.begin();
    ESP_LOGI(kTag,"RAM_SELF_TEST result=%s I2S=DISABLED restarted=%d",passed?"PASS":"FAIL",runtime.snapshot().ready);
    if(!passed) state.error=ESP_FAIL;
#endif
#endif
#if defined(CONFIG_SMKP4_PCM_WIRING_CONFIRMED)
    state.physical=true;
    if(state.error==ESP_OK) state.error=output.begin(CONFIG_SMKP4_PCM_BCLK, CONFIG_SMKP4_PCM_WS, CONFIG_SMKP4_PCM_DATA);
    if(state.error==ESP_OK) state.error=output.start();
#endif
    if(!tone.configure(CONFIG_SMKP4_PCM_TEST_HZ,kTestAmplitude)) state.error=ESP_ERR_INVALID_ARG;
    state.running=state.error==ESP_OK;
    state.engine=runtime.snapshot();
    state.owner_initialized=true;
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
        const int16_t* samples=pcm;
        const uint16_t previous_patch=runtime.snapshot().patch_id;
        const int64_t command_start=esp_timer_get_time();
        runtime.apply(static_cast<uint32_t>(command_start/1000));
        const uint32_t command_us=static_cast<uint32_t>(esp_timer_get_time()-command_start);
        state.command_total_us+=command_us;
        if(command_us>state.command_max_us) state.command_max_us=command_us;
        if(previous_patch!=runtime.snapshot().patch_id && command_us>state.patch_max_us) state.patch_max_us=command_us;
        const int64_t dsp_start=esp_timer_get_time();
#if defined(CONFIG_SMKP4_PCM_DIAGNOSTIC)
        if(!tone.render(pcm,kBlockFrames,sequence)) { state.error=ESP_FAIL; break; }
#else
        (void)sequence;
        samples=runtime.render();
        if(!samples || runtime.snapshot().oom_count) { state.error=ESP_FAIL; break; }
#endif
        const uint32_t render_us = static_cast<uint32_t>(esp_timer_get_time() - dsp_start);
        state.render_total_us += render_us;
        ++state.blocks; // Includes generated blocks whose subsequent I2S write fails.
        if (render_us > state.render_max_us) state.render_max_us = render_us;
        if (render_us * static_cast<uint64_t>(kSampleRateHz) > kBlockFrames * 1000000ULL)
            ++state.render_deadlines;
        const int64_t dma_start=esp_timer_get_time();
        if(state.physical) state.error = output.write(samples, kBlockFrames);
        const uint32_t dma_us=state.physical?static_cast<uint32_t>(esp_timer_get_time()-dma_start):0;
        state.dma_total_us+=dma_us;
        if(dma_us>state.dma_max_us) state.dma_max_us=dma_us;
        const uint32_t service_us = static_cast<uint32_t>(esp_timer_get_time() - start);
        if (service_us > state.service_max_us) state.service_max_us = service_us;
        if (state.error != ESP_OK) {
            ++state.write_errors;
            if (state.error == ESP_ERR_TIMEOUT) ++state.timeouts;
            break; // Stop clocks; never loop rapidly on a failed DMA channel.
        }
        if(state.physical) state.frames += kBlockFrames;
        state.engine=runtime.snapshot();
        state.stack_free_bytes = uxTaskGetStackHighWaterMark(nullptr);
        xQueueOverwrite(snapshots, &state); // Fixed single-element mailbox, no waiting.
        const uint32_t total_service_us=static_cast<uint32_t>(esp_timer_get_time()-start);
        if(total_service_us>state.service_max_us) state.service_max_us=total_service_us;
        // Include housekeeping/mailbox in the compute budget, excluding only
        // the measured I2S write. Published maxima can lag by one block.
        if((total_service_us-dma_us)*static_cast<uint64_t>(kSampleRateHz)>kBlockFrames*1000000ULL)
            ++state.service_deadlines;
        // Software-only sink is explicitly not a sample-rate/latency qualification.
        // Hardware production is paced solely by the I2S DMA write.
        if(!state.physical) vTaskDelay(pdMS_TO_TICKS(6)+1);
    }
    const esp_err_t stop_error = output.stop();
    if (state.error == ESP_OK) state.error = stop_error;
    runtime.end(); state.engine=runtime.snapshot();
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
    ESP_LOGI(kTag, "Standalone local AMY; USB opt-in; no UART, ES8311 or NS4150");
    ESP_LOGI(kTag, "PCM=%" PRIu32 "Hz signed16 stereo Philips; block=%u DMA=%ux%u MCLK=unused",
             kSampleRateHz, unsigned(kBlockFrames), kDmaDescriptors, unsigned(kBlockFrames));
    snapshots = xQueueCreateStatic(1, sizeof(Snapshot), snapshot_storage, &snapshot_control);
    ESP_LOGI(kTag, "Candidate I2S (driven only when wiring confirmed) BCLK=%d WS=%d DATA=%d test_sequence=%d frequency=%dHz peak=%.2f",
             CONFIG_SMKP4_PCM_BCLK, CONFIG_SMKP4_PCM_WS, CONFIG_SMKP4_PCM_DATA,
#if defined(CONFIG_SMKP4_PCM_TEST_SEQUENCE)
             1,
#else
             0,
#endif
             CONFIG_SMKP4_PCM_TEST_HZ, double(kTestAmplitude));
    if (!snapshots) ESP_LOGE(kTag,"Snapshot mailbox creation failed; diagnostics continue without audio/USB");
    const bool owner_started=snapshots && xTaskCreate(audioTask, "amy_owner", smk::p4::amy::kOwnerStackBytes, nullptr,
                                  smk::p4::amy::kOwnerPriority, nullptr) == pdPASS;
    if (!owner_started) {
        ESP_LOGE(kTag, "AUDIO FAIL: task/mailbox creation failed");
    }
    // Let the boot owner finish before display allocations. A failed display
    // never stops the independent audio owner. No serial upload in this task.
    Snapshot initial{};
#if defined(CONFIG_SMKP4_AMY_RAM_SELF_TEST)
    constexpr unsigned boot_polls=12000; // Explicit diagnostic can render long ROM releases.
#else
    constexpr unsigned boot_polls=200;
#endif
    const bool snapshot_received=smk::p4::boot::awaitSnapshot(snapshots!=nullptr,owner_started,initial,boot_polls,
        [](Snapshot& value) { return xQueuePeek(snapshots,&value,0)==pdTRUE; },
        [] { vTaskDelay(pdMS_TO_TICKS(10)); });
    if(!snapshot_received) ESP_LOGE(kTag,"Initial audio snapshot unavailable: %s; diagnostics continue; no automatic restart",
        !snapshots?"mailbox unavailable":owner_started?"timeout":"owner task not started");
#if defined(CONFIG_SMKP4_USB_WIRING_CONFIRMED)
    // This opt-in config records operator electrical qualification and enables
    // USB mode. Compilation never supplies that qualification.
    constexpr bool usb_enabled_and_qualified=true;
#else
    constexpr bool usb_enabled_and_qualified=false;
#endif
    const auto usb_start=smk::p4::boot::usbStart(usb_enabled_and_qualified,snapshots!=nullptr,
        owner_started,snapshot_received,initial);
    ESP_LOGI(kTag,"USB boot gate: %s",smk::p4::boot::reason(usb_start));
#if defined(CONFIG_SMKP4_USB_WIRING_CONFIRMED)
#if defined(CONFIG_SMKP4_USB_MIDI_TRACE)
    runtime.usbInput().enableTrace();
#endif
    if(usb_start==smk::p4::boot::UsbStart::Ready) {
        ESP_LOGI(kTag,"Starting qualified USB Host controller=%d",CONFIG_SMKP4_USB_CONTROLLER);
        if(!usb_host.begin(CONFIG_SMKP4_USB_CONTROLLER)) ESP_LOGE(kTag,"USB Host initialization failed; Touch remains available");
    } else ESP_LOGE(kTag,"USB Host not started: %s",smk::p4::boot::reason(usb_start));
#endif
#if defined(CONFIG_SMKP4_PCM_DIAGNOSTIC)
    constexpr bool diagnostic=true;
#else
    constexpr bool diagnostic=false;
#endif
    const bool display_ready = smk::p4::beginDisplayTest(diagnostic);
    static smk::p4::LocalAmyPerformanceBackend backend(runtime.commands());
    #if defined(CONFIG_SMKP4_UI_BLUEPRINT)
    if(snapshot_received) backend.publish(initial.engine,initial.running && initial.physical,initial.render_max_us,initial.render_deadlines);
    static smk::p4::UiBlueprintPerformanceAdapter adapter(backend);
    adapter.service(static_cast<uint32_t>(esp_timer_get_time()/1000));
    static smk::p4::ui::UiBlueprint ui(adapter);
#else
    static smk::p4::PerformanceUi ui(backend);
#endif
    const bool ui_ready=display_ready && !diagnostic && ui.begin();
    if(!display_ready || (!diagnostic && !ui_ready)) ESP_LOGE(kTag,"Display/UI failed; audio owner continues");
    TickType_t last_report = xTaskGetTickCount();
    for (;;) {
        Snapshot current{};
        if(snapshots && xQueuePeek(snapshots,&current,0)==pdTRUE)
            backend.publish(current.engine,current.running && current.physical,current.render_max_us,current.render_deadlines);
        backend.publishMidi(usb_host.state()==smk::p4::usb::Host::State::Connected,
            usb_host.state()==smk::p4::usb::Host::State::Error,runtime.usbInput().events.load());
        if (display_ready) smk::p4::serviceDisplayTest();
        if(ui_ready) ui.service(static_cast<uint32_t>(esp_timer_get_time()/1000));
        if (xTaskGetTickCount() - last_report >= pdMS_TO_TICKS(smk::p4::config::kDiagnosticsIntervalMs)) {
            last_report = xTaskGetTickCount();
            Snapshot state{};
            if (snapshots && xQueuePeek(snapshots, &state, 0) == pdTRUE) {
                ESP_LOGI(kTag, "AUDIO %s frames_written=%" PRIu64 " blocks_rendered=%" PRIu32
                         " render_avg_us=%" PRIu64 " render_max_us=%" PRIu32
                         " render_deadlines=%" PRIu32 " service_max_us=%" PRIu32
                         " refill_risks=%" PRIu32 " write_errors=%" PRIu32 " timeouts=%" PRIu32
                         " stack_free_bytes=%u error=%s DMA_underruns=UNMEASURED",
                         state.running ? (state.physical?"I2S_PRODUCING":"RAM_ONLY") : "FAIL", state.frames, state.blocks,
                         state.blocks ? state.render_total_us / state.blocks : 0, state.render_max_us,
                         state.render_deadlines, state.service_max_us, state.refill_risks,
                         state.write_errors, state.timeouts, state.stack_free_bytes, esp_err_to_name(state.error));
                ESP_LOGI(kTag,"ENGINE ready=%d patch=%u notes=%lu peak=%lu clipped=%lu oom=%lu queue=%lu high=%lu overflow=%lu expired=%lu latency_max_ms=%lu cmd_avg/max=%llu/%lu dsp_avg/max=%llu/%lu compute_deadlines=%lu patch_max_us=%lu dma_avg/max=%llu/%lu",
                    state.engine.ready,state.engine.patch_id,(unsigned long)state.engine.active_notes,
                    (unsigned long)state.engine.peak,(unsigned long)state.engine.clipped,(unsigned long)state.engine.oom_count,
                    (unsigned long)runtime.commands().depth(),(unsigned long)state.engine.queue_high_water,(unsigned long)runtime.commands().overflows(),
                    (unsigned long)state.engine.expired_notes,(unsigned long)state.engine.command_latency_max_ms,
                    (unsigned long long)(state.blocks?state.command_total_us/state.blocks:0),(unsigned long)state.command_max_us,
                    (unsigned long long)(state.blocks?state.render_total_us/state.blocks:0),(unsigned long)state.render_max_us,
                    (unsigned long)state.service_deadlines,(unsigned long)state.patch_max_us,
                    (unsigned long long)(state.blocks?state.dma_total_us/state.blocks:0),(unsigned long)state.dma_max_us);
            }
            smk::p4::reportMemory();
#if defined(CONFIG_SMKP4_UI_BLUEPRINT)
            if(ui_ready) {
                lv_mem_monitor_t memory{}; lv_mem_monitor(&memory);
                auto& d=adapter.model().diagnostics();
                d.memory_valid=true;
                d.lvgl_free_bytes=memory.free_size; d.lvgl_largest_block=memory.free_biggest_size;
                d.lvgl_peak_used=memory.max_used; d.lvgl_frag_pct=memory.frag_pct;
                d.free_internal_ram=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
                d.free_psram=heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
                ESP_LOGI(kTag,"UI_BLUEPRINT stack_free_bytes=%u screen_switches=%lu",
                    unsigned(uxTaskGetStackHighWaterMark(nullptr)),(unsigned long)d.screen_switches);
                adapter.model().bumpRevision();
            }
#endif
#if defined(CONFIG_SMKP4_USB_MIDI_TRACE)
            smk::p4::amy::UsbInput::Trace trace;
            for(unsigned i=0;i<4 && runtime.usbInput().popTrace(trace);++i)
                ESP_LOGI(kTag,"MIDI_TRACE t_ms=%lu bytes=%u cable=%u channel=%u packet=%02x %02x %02x %02x trace_dropped=%lu",
                    (unsigned long)trace.timestamp_ms,unsigned(trace.valid_bytes),unsigned(trace.packet[0]>>4),unsigned((trace.packet[1]&15)+1),
                    trace.packet[0],trace.packet[1],trace.packet[2],trace.packet[3],(unsigned long)runtime.usbInput().trace_dropped.load());
#endif
            ESP_LOGI(kTag,"USB state=%u connects=%lu disconnects=%lu errors=%lu events=%lu parse_errors=%lu ignored_cable_packets=%lu dropped=%lu queue=%lu overflow=%lu stack_free=%lu polyphony_rejected=%lu",
                unsigned(usb_host.state()),(unsigned long)usb_host.connects(),(unsigned long)usb_host.disconnects(),
                (unsigned long)usb_host.errors(),(unsigned long)runtime.usbInput().events.load(),
                (unsigned long)runtime.usbInput().parse_errors.load(),(unsigned long)runtime.usbInput().ignored_cable_packets.load(),
                (unsigned long)runtime.usbInput().dropped.load(),
                (unsigned long)runtime.usbInput().commands.depth(),(unsigned long)runtime.usbInput().commands.overflows(),
                (unsigned long)usb_host.stackFree(),(unsigned long)state.engine.polyphony_rejected);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
