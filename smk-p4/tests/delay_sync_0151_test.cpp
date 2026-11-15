#include "amy_runtime.h"
#include "global_tempo_nvs.h"
#include "fake_nvs.h"
#include "performance_factory_routes.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <chrono>

extern "C" void probe_realtime(bool);
extern "C" uint32_t probe_calls();
using namespace smk::p4;

static void storage() {
    fx::FxConfig original{};
    original.delay_time_ms=421;
    original.delay_mode=fx::DelayMode::Sync;
    original.delay_division=tempo::Division::DottedSixteenth;
    uint8_t bytes[32]{};
    assert(fx::encodeRecord(original,9,bytes));
    uint32_t sequence=0;
    fx::FxConfig decoded{};
    assert(fx::decodeRecord(bytes,sizeof(bytes),decoded,sequence));
    assert(sequence==9 && decoded.delay_time_ms==421 && decoded.delay_mode==original.delay_mode &&
           decoded.delay_division==original.delay_division);
    fx::Record record{};
    std::memcpy(&record,bytes,sizeof(record));
    record.reserved[1]=255;
    record.crc32=fx::calcCrc(reinterpret_cast<const uint8_t*>(&record),28);
    assert(!fx::decodeRecord(reinterpret_cast<const uint8_t*>(&record),sizeof(record),decoded,sequence));
    record.version=1;
    std::memset(record.reserved,0,sizeof(record.reserved));
    record.crc32=fx::calcCrc(reinterpret_cast<const uint8_t*>(&record),28);
    assert(fx::decodeRecord(reinterpret_cast<const uint8_t*>(&record),sizeof(record),decoded,sequence));
    assert(decoded.delay_mode==fx::DelayMode::Free && decoded.delay_time_ms==421);

    const tempo::NvsTempo store;
    assert(store.load()==120);
    assert(!store.save(241));
    assert(store.save(173) && store.load()==173);
    assert(fake_nvs::writes==1);
    fake_nvs::blobs.at({2,"global"})[7]^=1;
    assert(store.load()==120);
}

static void macroRouting() {
    edit::State edit{};
    edit.performance_routes.custom=true;
    edit.performance_routes.routes[0][0]=performance::route(performance::DelayTime,10,1000,0.40f);
    edit.performance_routes.routes[0][1]=performance::route(performance::DelayMix,0,1,0.30f);
    edit.performance_macros[0]=127;
    fx::FxConfig manual{};
    const auto free=performance::effectiveFx(manual,edit);
    manual.delay_mode=fx::DelayMode::Sync;
    const auto synced=performance::effectiveFx(manual,edit);
    assert(free.delay_time_ms>manual.delay_time_ms && synced.delay_time_ms==manual.delay_time_ms &&
           free.delay_mix==synced.delay_mix && synced.delay_mix>0);
}

int main() {
    storage();
    macroRouting();
    amy::Runtime runtime;
    assert(runtime.begin());
    const auto& state=runtime.snapshot();
    auto& queue=runtime.commands();
    const uint32_t allocations=probe_calls(),oom=state.oom_count;
    uint32_t now=0;
    int peak=0, delta=0, previous[2]{};
    double elapsed_us=0,worst_us=0;
    unsigned blocks=0;
    auto render=[&](unsigned count) {
        using Clock=std::chrono::steady_clock;
        for(unsigned i=0;i<count;++i) {
            probe_realtime(true);
            const auto start=Clock::now();
            runtime.apply(now);
            const auto* pcm=runtime.render();
            const double duration=std::chrono::duration<double,std::micro>(Clock::now()-start).count();
            probe_realtime(false);
            elapsed_us+=duration;
            worst_us=std::max(worst_us,duration);
            ++blocks;
            assert(pcm);
            for(unsigned j=0;j<amy::kBlockFrames*2;++j) {
                peak=std::max(peak,std::abs(int(pcm[j])));
                delta=std::max(delta,std::abs(int(pcm[j])-previous[j%2]));
                previous[j%2]=pcm[j];
            }
            now+=5;
        }
    };
    const auto bpm=[&](unsigned value) {
        amy::Command c{amy::CommandType::Arp,uint16_t(arp::Action::Bpm),uint8_t(value),0,queue.nextToken(),now};
        c.version=arp::kProtocolVersion;
        assert(queue.push(c));
    };
    fx::FxRequest request{};
    request.config.delay_time_ms=321;
    request.config.delay_mix=0.26f;
    request.config.delay_feedback=0.95f;
    request.config.chorus_mode=1;
    request.config.reverb_mix=0.16f;
    const auto applyFx=[&] { ++request.request;assert(queue.fx.push(request)); };
    applyFx();
    for(unsigned i=0;i<8;++i)
        assert(queue.push({amy::CommandType::NoteOn,uint16_t(60+i),uint8_t(38+i),0,queue.nextToken(),now}));
    render(120);
    assert(state.fx.delay_dsp_ms==321 && state.active_notes==8 && state.arp.tempo.tick()>0);
    request.config.delay_mode=fx::DelayMode::Sync;
    request.config.delay_division=tempo::Division::Quarter;
    applyFx();
    render(1);
    assert(state.fx.delay_target_ms==500 && state.fx.delay_dsp_ms==321);
    render(100);
    assert(state.fx.delay_dsp_ms==500);
    for(unsigned rate:{100,140,60,40,100}) {
        const auto phase=state.arp.tempo.phase;
        bpm(rate);render(110);
        assert(state.arp.tempo.bpm==rate && state.arp.tempo.phase-phase==110ull*amy::kBlockFrames*rate*tempo::kPpqn*25);
        assert(state.fx.config.delay_time_ms==321);
        if(rate==40) assert(state.fx.delay_out_of_range && state.fx.delay_dsp_ms==1000);
        else assert(!state.fx.delay_out_of_range &&
                    std::abs(state.fx.delay_dsp_ms-60000.0f/rate)<=1.0f/48);
    }
    for(unsigned i=0;i<24;++i) {
        request.config.delay_division=tempo::Division(i%unsigned(tempo::Division::Count));
        request.config.bypass_all=(i%3)==0;
        request.config.delay_mode=i%2?fx::DelayMode::Sync:fx::DelayMode::Free;
        applyFx();render(3);
    }
    request.config.delay_mode=fx::DelayMode::Free;
    request.config.bypass_all=false;
    applyFx();render(25);
    assert(state.fx.delay_dsp_ms==321);
    const auto phase=state.arp.tempo.phase;
    queue.panic();render(1);
    assert(state.arp.tempo.phase>phase && !state.active_notes);
    assert(state.oom_count==oom && probe_calls()==allocations && peak>0 && !state.clipped);
    std::printf("PASS delay transition: blocks=%u apply_render_avg_us=%.2f max_us=%.2f peak=%d max_sample_delta=%d clipping=%u oom=%u\n",
                blocks,elapsed_us/blocks,worst_us,peak,delta,state.clipped,state.oom_count);
}
