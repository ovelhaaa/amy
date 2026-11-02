#include "remote_link.h"
#include "smk_state.h"
#include "audio_config.h"
#include "audio_task.h"
#include "diagnostics.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace smk {
static const char* TAG="P4_LINK";
bool RemoteLink::begin() {
#if defined(CONFIG_SMK_LINK_ENABLED)
    const esp_err_t error=uart_.begin(CONFIG_SMK_LINK_UART,CONFIG_SMK_LINK_TX_GPIO,CONFIG_SMK_LINK_RX_GPIO);
    if(error!=ESP_OK) { ESP_LOGE(TAG,"UART init failed: %s; synth remains independent",esp_err_to_name(error)); return false; }
    ready_=true;
    ESP_LOGI(TAG,"Control UART%d enabled at %d baud; no PCM, RECEIVED acknowledgements only",CONFIG_SMK_LINK_UART,link::kBaud);
    return true;
#else
    return false;
#endif
}
void RemoteLink::releaseAudition() { events_.cancelNotes(EventSource::RemoteUi); }
link::Error RemoteLink::receive(const link::Frame& f) {
    if(!link::validCommand(f)) return link::Error::Payload;
    SynthEvent e{}; e.source=EventSource::RemoteUi;
    e.timestamp_us=static_cast<uint64_t>(esp_timer_get_time());
    switch(f.type) {
        case link::Type::SetPatch: e.type=EventType::PatchChange; e.id=link::get16(f.payload); break;
        case link::Type::SetMacro:
            if(!patches_.activePatch().macros[f.payload[0]].mapping_count) return link::Error::Unsupported;
            e.type=EventType::ParameterChange; e.id=f.payload[0]; e.value=f.payload[1]; break;
        case link::Type::SetMasterVolume:
            e.type=EventType::ParameterChange; e.id=8; e.value=f.payload[0]; break;
        case link::Type::Panic: e.type=EventType::Panic; break;
        case link::Type::AuditionNoteOn:
            e.type=EventType::NoteOn; e.id=f.payload[0]; e.value=f.payload[1]; break;
        case link::Type::AuditionNoteOff:
            // MVP is monophonic touch. Selective cancellation is reserved even
            // when both EventBus and AMY command FIFOs are completely full.
            releaseAudition(); return link::Error::None;
        default: return link::Error::Unsupported;
    }
    return events_.send(e)?link::Error::None:link::Error::Busy;
}
void RemoteLink::snapshot(link::Frame& response) {
    link::State s{};
    const auto& patch=patches_.activePatch();
    s.patch_id=patch.id; s.engine_patch=patch.engine_patch;
    std::memcpy(s.patch_name,patch.name,24);
    s.master=static_cast<uint8_t>(std::lround(std::clamp(engine_.masterGain(),0.0f,1.0f)*127.0f));
    s.bpm_centi=static_cast<uint16_t>(std::lround(std::clamp(clock_.bpm(),0.0f,655.0f)*100.0f));
    s.sample_rate_hz=config::kSampleRateHz; s.block_frames=config::kBlockSize;
    auto& c=Diagnostics::instance().counters();
    const uint32_t frames=c.frames_rendered.load(std::memory_order_relaxed);
    if(frames!=last_frames_) { last_frames_=frames; last_frame_ms_=now_ms_; }
    s.flags=link::kControlState | (c.usb_connected.load()?link::kUsbConnected:0);
    if(frames && now_ms_-last_frame_ms_<500 && !AudioTask::failed()) s.flags|=link::kAudioLive;
    s.max_render_us=c.max_render_us.load(); s.deadline_misses=c.render_deadline_misses.load();
    s.pcm_starvations=c.synth_pcm_starvations.load(); s.event_overflows=c.event_queue_overflows.load();
    s.commands_dropped=c.synth_commands_dropped.load(); s.midi_events=c.events_received.load();
    s.active_notes=engine_.activeNoteReferences();
    s.free_internal=static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
    s.free_psram=static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    for(unsigned i=0;i<8;++i) {
        std::memcpy(s.macros[i].name,patch.macros[i].name,8);
        s.macros[i].value=static_cast<uint8_t>(std::lround(std::clamp(patch.macros[i].current_val,0.0f,127.0f)));
        s.macros[i].supported=patch.macros[i].mapping_count>0;
    }
    link::writeState(response,s);
}
bool RemoteLink::patch(uint16_t id,link::Frame& response) {
    if(id>=256) return false;
    const auto* p=FactoryPatches::getPatchById(static_cast<uint8_t>(id));
    if(!p) return false;
    response.size=28; link::put16(response.payload,p->id); link::put16(response.payload+2,p->engine_patch);
    std::memcpy(response.payload+4,p->name,24); return true;
}
void RemoteLink::service(uint32_t now_ms) {
    if(!ready_) return;
    now_ms_=now_ms; server_.tick(now_ms);
    link::Frame request{};
    const bool received=uart_.poll(now_ms,request);
    if(uart_.receiveErrors()!=uart_errors_) {
        uart_errors_=uart_.receiveErrors(); server_.disconnect();
    }
    if(received) {
        const auto response=server_.handle(request,now_ms);
        uart_.send(response,request.type==link::Type::Panic || request.type==link::Type::AuditionNoteOff);
    }
    if(server_.connected() && now_ms-last_diagnostics_ms_>=2000) {
        last_diagnostics_ms_=now_ms;
        link::Frame telemetry{}; telemetry.type=link::Type::AudioDiagnostics;
        telemetry.session=server_.session(); telemetry.size=link::kDiagnosticsBytes;
        auto& c=Diagnostics::instance().counters();
        link::put32(telemetry.payload,config::kSampleRateHz); link::put16(telemetry.payload+4,config::kBlockSize);
        const uint32_t values[]={c.max_render_us.load(),c.render_deadline_misses.load(),c.synth_pcm_starvations.load(),
            c.i2s_write_errors.load(),c.i2s_write_timeouts.load(),
            static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),
            static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)),engine_.activeNoteReferences()};
        for(unsigned i=0;i<8;++i) link::put32(telemetry.payload+6+i*4,values[i]);
        uart_.send(telemetry);
    }
    if(now_ms-last_log_ms_>=5000) {
        last_log_ms_=now_ms;
        ESP_LOGI(TAG,"connected=%d rx_errors=%lu bad_frames=%lu tx_drops=%lu rejected=%lu reconnects=%lu",
                 server_.connected(),static_cast<unsigned long>(uart_errors_),static_cast<unsigned long>(uart_.frameErrors()),
                 static_cast<unsigned long>(uart_.transmitDrops()),static_cast<unsigned long>(server_.rejected()),
                 static_cast<unsigned long>(server_.reconnects()));
    }
}
}
