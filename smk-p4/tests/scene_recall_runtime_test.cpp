#include "amy_runtime.h"
#include "fake_nvs.h"
#include "local_performance_backend.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>

extern "C" void probe_realtime(bool);
extern "C" void probe_fail_next(unsigned);
extern "C" void probe_on_allocation(unsigned,void (*)());
extern "C" uint32_t probe_calls();
extern "C" uint32_t probe_live();
extern "C" size_t probe_bytes();
extern "C" size_t probe_peak_bytes();
extern "C" uint64_t probe_allocated_bytes();
extern "C" uint64_t probe_released_bytes();

using namespace smk::p4;

int main() {
    static amy::Runtime runtime;
    const auto initial_bytes=probe_bytes();
    const auto initial_live=probe_live();
    assert(runtime.begin());
    auto& commands=runtime.commands();
    LocalAmyPerformanceBackend backend(commands);
    auto makeRequest=[&](uint8_t mask) {
        const auto& snapshot=runtime.snapshot();
        scene::RecallRequest request;
        std::strcpy(request.configuration.name,"Scene");
        request.configuration.domains=request.requested=mask;
        request.configuration.bpm=240;
        request.configuration.fx.delay_mix=0.35f;
        request.configuration.harmony.root=7;
        request.configuration.arp.enabled=true;
        request.configuration.arp.mode=arp::Mode::Down;
        request.generation=commands.generation();
        request.token=commands.nextToken();
        request.fx_revision=snapshot.fx.revision;
        request.harmony_revision=snapshot.harmony_revision;
        request.arp_revision=snapshot.arp.revision;
        request.tempo_revision=snapshot.arp.tempo.revision;
        request.edit_revision=snapshot.edit.revision;
        request.expected_pattern={snapshot.sequencer_pattern,snapshot.sequencer.division,
                                  snapshot.sequencer.swing,snapshot.sequencer.transpose,
                                  snapshot.sequencer.quantize};
        request.expected_pattern_slot=snapshot.sequencer.active_slot;
        request.apply_tempo=true;
        request.slot=3;
        return request;
    };
    unsigned block=0;
    using Clock=std::chrono::steady_clock;
    using Micros=std::chrono::duration<double,std::micro>;
    double apply_us=0,render_us=0,max_apply_us=0,max_render_us=0;
    unsigned measured_blocks=0;
    auto advance=[&]() {
        const auto begin=Clock::now();
        probe_realtime(true);
        runtime.apply(block++*amy::kBlockFrames*1000/amy::kSampleRateHz);
        const auto applied=Clock::now();
        assert(runtime.render());
        probe_realtime(false);
        const double a=Micros(applied-begin).count(),r=Micros(Clock::now()-applied).count();
        apply_us+=a;
        render_us+=r;
        max_apply_us=std::max(max_apply_us,a);
        max_render_us=std::max(max_render_us,r);
        ++measured_blocks;
    };
    auto request=makeRequest(scene::Fx|scene::Harmony|scene::Arp|scene::Tempo);
    backend.publish(runtime.snapshot(),true,0,0);
    const auto backend_token=backend.submitScene(request);
    assert(backend_token);
    assert(!commands.scenes.push(request));
    advance();
    const auto& snapshot=runtime.snapshot();
    backend.publish(snapshot,true,0,0);
    assert(backend.sceneStatus().token==backend_token &&
           backend.sceneStatus().phase==scene::RecallPhase::Confirmed);
    assert(snapshot.scene.phase==scene::RecallPhase::Confirmed);
    assert(snapshot.scene.revision==1 && snapshot.scene.active==3);
    assert(snapshot.fx.config.delay_mix==0.35f && snapshot.fx.applied.delay_mix==0.35f);
    assert(snapshot.harmony.root==7 && snapshot.arp.tempo.bpm==240);
    assert(snapshot.arp.config.enabled && snapshot.arp.config.mode==arp::Mode::Down);

    request=makeRequest(scene::Fx|scene::Harmony|scene::Tempo);
    request.locks=perf_lock::Fx|perf_lock::Harmony;
    request.mode=scene::RecallMode::NextBar;
    request.configuration.bpm=40;
    request.configuration.fx.delay_mix=0.7f;
    request.configuration.harmony.root=8;
    request.slot=4;
    assert(commands.scenes.push(request));
    advance();
    assert(snapshot.scene.phase==scene::RecallPhase::WaitingBoundary);
    assert(snapshot.scene.pending==4 && snapshot.scene.effective==scene::Tempo);
    assert(snapshot.arp.tempo.bpm==240);
    for(unsigned i=0;i<200 && snapshot.scene.phase==scene::RecallPhase::WaitingBoundary;++i) advance();
    assert(snapshot.scene.phase==scene::RecallPhase::Confirmed);
    assert(snapshot.scene.active==4 && snapshot.scene.revision==2 && snapshot.arp.tempo.bpm==40);
    assert(snapshot.fx.config.delay_mix==0.35f && snapshot.harmony.root==7);
    const auto late=snapshot.scene.applied_phase-snapshot.scene.due_phase;
    const auto units_per_frame=tempo::Position{0,0,240}.unitsPerFrame();
    assert(late<units_per_frame*amy::kBlockFrames);

    request=makeRequest(scene::Tempo);
    request.mode=scene::RecallMode::NextBar;
    assert(commands.scenes.push(request));
    advance();
    commands.scenes.cancel(request.token);
    advance();
    assert(snapshot.scene.phase==scene::RecallPhase::Cancelled);
    assert(snapshot.scene.active==4 && snapshot.scene.revision==2);

    request=makeRequest(scene::Tempo);
    request.mode=scene::RecallMode::NextBar;
    assert(commands.scenes.push(request));
    advance();
    seq::Request stop;
    stop.action=seq::Action::Stop;
    stop.token=commands.nextToken();
    assert(commands.pushSequencer(stop));
    advance();
    assert(snapshot.scene.phase==scene::RecallPhase::Cancelled);
    assert(snapshot.scene.active==4 && snapshot.arp.tempo.bpm==40);

    request=makeRequest(scene::Tempo);
    request.mode=scene::RecallMode::NextBar;
    assert(commands.scenes.push(request));
    advance();
    commands.panic();
    advance();
    assert(snapshot.scene.phase==scene::RecallPhase::Cancelled);
    assert(snapshot.active_notes==0 && snapshot.arp.tempo.bpm==40);

    request=makeRequest(scene::Fx);
    ++request.fx_revision;
    assert(commands.scenes.push(request));
    advance();
    assert(snapshot.scene.phase==scene::RecallPhase::Failed);
    assert(snapshot.scene.error==scene::RecallError::Stale);
    assert(snapshot.scene.revision==2);

    request=makeRequest(scene::Fx|scene::Patch);
    request.configuration.patch_id=UINT16_MAX;
    assert(commands.scenes.push(request));
    advance();
    assert(snapshot.scene.rejection==scene::Reject::PatchUnavailable);
    assert(snapshot.fx.config.delay_mix==0.35f);

    request=makeRequest(scene::Macros);
    request.configuration.macros[0]=20;
    assert(commands.scenes.push(request));
    advance();
    assert(snapshot.scene.phase==scene::RecallPhase::Confirmed);
    assert(snapshot.scene.applied_domains==scene::Macros);
    assert(snapshot.edit.performance_macros[0]==20 && snapshot.edit.performance_mask);

    request=makeRequest(scene::Macros|scene::Fx);
    request.locks=perf_lock::Fx;
    request.configuration.macros[2]=127;
    assert(commands.scenes.push(request));
    advance();
    assert(snapshot.scene.error==scene::RecallError::FxConflict);
    assert(snapshot.scene.applied_domains==0 && snapshot.edit.performance_macros[0]==20);
    assert(snapshot.fx.applied.delay_mix==0.35f);

    request=makeRequest(scene::Macros|scene::Fx);
    request.configuration.macros[2]=127;
    assert(commands.scenes.push(request));
    advance();
    assert(snapshot.scene.phase==scene::RecallPhase::Confirmed);
    assert(snapshot.scene.applied_domains==(scene::Macros|scene::Fx));
    assert(snapshot.fx.config.delay_mix==0.35f && snapshot.fx.applied.delay_mix>0.35f);

    const auto note=commands.nextToken();
    assert(commands.push({amy::CommandType::NoteOn,60,100,0,note,block*amy::kBlockFrames*1000/amy::kSampleRateHz}));
    advance();
    assert(snapshot.active_notes==1);
    request=makeRequest(scene::Pattern);
    request.configuration.pattern_slot=5;
    request.pattern.state=seq::SlotState::Valid;
    request.pattern.sequence=9;
    request.pattern.settings.pattern.length=3;
    request.pattern.settings.pattern.steps[0]={64,100,50,true};
    request.discard_pattern=true;
    assert(commands.scenes.push(request));
    advance();
    assert(snapshot.scene.phase==scene::RecallPhase::Confirmed && snapshot.scene.pattern_sequence==9);
    assert(snapshot.sequencer.active_slot==5 && snapshot.sequencer_pattern.length==3);
    assert(snapshot.active_notes==1);
    commands.release(60,note);
    advance();
    assert(snapshot.active_notes==0);

    request=makeRequest(scene::Pattern);
    request.configuration.pattern_slot=5;
    assert(commands.scenes.push(request));
    advance();
    assert(snapshot.scene.rejection==scene::Reject::PatternUnavailable);

    for(const auto bpm:{40,120}) {
        request=makeRequest(scene::Tempo);
        request.configuration.bpm=uint16_t(bpm);
        assert(commands.scenes.push(request));
        advance();
        assert(snapshot.scene.phase==scene::RecallPhase::Confirmed);
        request=makeRequest(scene::Tempo);
        request.configuration.bpm=180;
        request.mode=scene::RecallMode::NextBar;
        assert(commands.scenes.push(request));
        advance();
        assert(snapshot.scene.phase==scene::RecallPhase::WaitingBoundary);
        assert(snapshot.arp.tempo.bpm==bpm);
        for(unsigned i=0;i<1200 && snapshot.scene.phase==scene::RecallPhase::WaitingBoundary;++i)
            advance();
        assert(snapshot.scene.phase==scene::RecallPhase::Confirmed && snapshot.arp.tempo.bpm==180);
        const auto lateness=snapshot.scene.applied_phase-snapshot.scene.due_phase;
        const auto speed=tempo::Position{0,0,uint16_t(bpm)}.unitsPerFrame();
        assert(lateness<speed*amy::kBlockFrames);
        std::printf("PASS Next Bar at %d BPM, lateness %.2f frames\n",bpm,double(lateness)/speed);
    }
    request=makeRequest(scene::kAll);
    request.configuration.patch_id=4;
    request.configuration.macros[0]=110;
    request.configuration.macros[2]=127;
    request.configuration.routes={performance::kFactoryRoutes,true};
    request.configuration.routes.routes[0][4]=performance::route(edit::Detune,-100,100,20,performance::Juno);
    request.configuration.pattern_slot=5;
    request.pattern.state=seq::SlotState::Valid;
    request.pattern.sequence=9;
    request.pattern.settings=request.expected_pattern;
    request.pattern.settings.pattern.length=6;
    request.discard_pattern=true;
    const auto full_start=Clock::now();
    assert(commands.scenes.push(request));
    advance();
    assert(snapshot.scene.phase==scene::RecallPhase::WaitingPatch);
    assert(snapshot.scene.generation==request.generation+1);
    const auto worker_start=Clock::now();
    runtime.loadPendingPatch();
    const auto worker_end=Clock::now();
    assert(snapshot.scene.generation==request.generation+2);
    assert(snapshot.scene.applied_domains==scene::Patch);
    advance();
    assert(snapshot.scene.phase==scene::RecallPhase::Confirmed);
    assert(snapshot.scene.applied_domains==scene::kAll);
    assert(snapshot.patch_id==4 && snapshot.sequencer_pattern.length==6);
    assert(snapshot.edit.performance_macros==request.configuration.macros);
    assert(snapshot.edit.performance_routes==request.configuration.routes);
    assert(snapshot.edit.performance_mask && snapshot.fx.applied.delay_mix>0.35f);
    assert(snapshot.harmony.root==7 && snapshot.arp.config.mode==arp::Mode::Down);
    assert(snapshot.arp.tempo.bpm==240 && snapshot.active_notes==0);
    std::printf("MEASURE full recall %.1f us, AMY patch worker %.1f us, two owner blocks\n",
                Micros(Clock::now()-full_start).count(),Micros(worker_end-worker_start).count());

    request=makeRequest(scene::Patch|scene::Tempo|scene::Macros|scene::Fx);
    request.configuration.patch_id=128;
    request.configuration.bpm=120;
    const auto confirmed_revision=snapshot.scene.revision;
    for(uint16_t pitch=60;pitch<68;++pitch) {
        const auto now=block*amy::kBlockFrames*1000/amy::kSampleRateHz;
        assert(commands.push({amy::CommandType::NoteOn,pitch,100,0,commands.nextToken(),now}));
    }
    advance();
    assert(snapshot.active_notes==8);
    assert(commands.scenes.push(request));
    advance();
    assert(snapshot.scene.phase==scene::RecallPhase::WaitingPatch);
    assert(snapshot.active_notes==0);
    runtime.loadPendingPatch();
    commands.panic();
    advance();
    assert(snapshot.scene.phase==scene::RecallPhase::PartialApply);
    assert(snapshot.scene.applied_domains==scene::Patch && snapshot.patch_id==128);
    assert(snapshot.scene.revision==confirmed_revision && snapshot.arp.tempo.bpm==240);
    assert(snapshot.active_notes==0);
    std::puts("PASS full recall through two patch generations and partial apply after Panic");

    request=makeRequest(scene::Patch|scene::Tempo|scene::Macros|scene::Fx);
    request.configuration.patch_id=4;
    request.configuration.bpm=120;
    assert(commands.scenes.push(request));
    advance();
    assert(snapshot.scene.phase==scene::RecallPhase::WaitingPatch);
    probe_fail_next(1);
    runtime.loadPendingPatch();
    assert(snapshot.scene.phase==scene::RecallPhase::Failed);
    assert(snapshot.scene.error==scene::RecallError::PatchFailed && snapshot.scene.patch_error);
    assert(snapshot.scene.applied_domains==0 && snapshot.scene.revision==confirmed_revision);
    assert(snapshot.patch_state==amy::PatchState::Failed && snapshot.active_notes==0);
    assert(commands.push({amy::CommandType::Patch,128}));
    advance();
    runtime.loadPendingPatch();
    advance();
    assert(snapshot.patch_state==amy::PatchState::Ready && snapshot.patch_id==128);
    assert(snapshot.active_notes==0 && snapshot.arp.tempo.bpm==240);
    std::puts("PASS scene patch allocation failure and recovery through existing loader");

    const auto startPatch=[&] {
        auto pending=makeRequest(scene::Patch);
        pending.configuration.patch_id=4;
        assert(commands.scenes.push(pending));
        advance();
        assert(snapshot.scene.phase==scene::RecallPhase::WaitingPatch);
    };
    const auto retryPatch=[&] {
        assert(commands.push({amy::CommandType::Patch,128}));
        advance();
        runtime.loadPendingPatch();
        advance();
        assert(snapshot.patch_state==amy::PatchState::Ready && snapshot.active_notes==0);
    };
    startPatch();
    block+=amy::kSampleRateHz*amy::kRecallTimeoutMs/(1000*amy::kBlockFrames)+1;
    advance();
    assert(snapshot.scene.error==scene::RecallError::PatchTimeout && snapshot.patch_state==amy::PatchState::Failed);
    retryPatch();

    startPatch();
    commands.storage_requested.store(true);
    advance();
    assert(snapshot.scene.phase==scene::RecallPhase::Cancelled && snapshot.storage_quiet);
    commands.storage_requested.store(false);
    advance();
    retryPatch();

    startPatch();
    runtime.usbInput().disconnect();
    runtime.loadPendingPatch();
    assert(snapshot.scene.phase==scene::RecallPhase::Cancelled && snapshot.active_notes==0);
    retryPatch();

    startPatch();
    assert(commands.push({amy::CommandType::Patch,135}));
    advance();
    runtime.loadPendingPatch();
    assert(snapshot.scene.phase==scene::RecallPhase::Cancelled && snapshot.patch_id==135);

    startPatch();
    commands.panic();
    runtime.loadPendingPatch();
    assert(snapshot.scene.phase==scene::RecallPhase::Cancelled && snapshot.active_notes==0);
    retryPatch();
    startPatch();
    auto patch_stop=seq::Request{};
    patch_stop.action=seq::Action::Stop;
    patch_stop.token=commands.nextToken();
    assert(commands.pushSequencer(patch_stop));
    advance();
    assert(snapshot.scene.phase==scene::RecallPhase::Cancelled && snapshot.active_notes==0);
    assert(snapshot.sequencer.stop.token==patch_stop.token && !snapshot.sequencer.stop.rejected);
    retryPatch();
    std::puts("PASS patch wait timeout, Storage Quiet, USB disconnect, external patch, Stop and generation cancellation");
    for(const auto& patch:amy::kPatches) {
        if(snapshot.patch_id!=patch.id) {
            assert(commands.push({amy::CommandType::Patch,patch.id}));
            advance();
            runtime.loadPendingPatch();
        }
        assert(snapshot.patch_id==patch.id && snapshot.patch_state==amy::PatchState::Ready);
        assert(snapshot.edit.available==edit::factoryPatchCapabilities(patch.id));
    }
    std::printf("PASS %zu factory capability masks against materialized AMY topology\n",amy::kPatchCount);
    for(const uint8_t domain:{scene::Fx,scene::Harmony,scene::Arp}) {
        request=makeRequest(domain);
        assert(commands.scenes.push(request));
        advance();
        assert(snapshot.scene.phase==scene::RecallPhase::Confirmed && snapshot.scene.applied_domains==domain);
    }
    for(const uint8_t companion:{uint8_t(0),uint8_t(scene::Pattern),uint8_t(scene::Fx),
            uint8_t(scene::Harmony),uint8_t(scene::Arp),uint8_t(scene::Macros)}) {
        request=makeRequest(scene::Patch|companion);
        request.configuration.patch_id=snapshot.patch_id==4?128:4;
        request.configuration.pattern_slot=5;
        request.pattern.state=seq::SlotState::Valid;
        request.pattern.sequence=9;
        request.pattern.settings=request.expected_pattern;
        request.discard_pattern=true;
        if(companion==scene::Macros) request.configuration.macros[0]=20;
        if(companion==scene::Fx) request.locks=perf_lock::Fx;
        const auto preserved_fx=snapshot.fx.config;
        assert(commands.scenes.push(request));
        advance();
        assert(snapshot.scene.phase==scene::RecallPhase::WaitingPatch);
        runtime.loadPendingPatch();
        advance();
        assert(snapshot.scene.phase==scene::RecallPhase::Confirmed);
        assert(snapshot.patch_id==request.configuration.patch_id);
        assert(snapshot.scene.applied_domains==(scene::Patch|(request.locks?0:companion)));
        if(request.locks) assert(fx::fxEquivalent(snapshot.fx.config,preserved_fx));
    }
    std::puts("PASS isolated domains and PATCH paired with PATTERN, locked FX, HARMONY, ARP and MACROS");
    for(const uint16_t bpm:{40,120,240}) {
        request=makeRequest(scene::Tempo);
        request.configuration.bpm=bpm;
        assert(commands.scenes.push(request));
        advance();
        request=makeRequest(scene::Patch);
        request.configuration.patch_id=snapshot.patch_id==4?128:4;
        request.mode=scene::RecallMode::NextBar;
        assert(commands.scenes.push(request));
        advance();
        assert(snapshot.scene.phase==scene::RecallPhase::WaitingBoundary);
        for(unsigned i=0;i<1200 && snapshot.scene.phase==scene::RecallPhase::WaitingBoundary;++i) advance();
        assert(snapshot.scene.phase==scene::RecallPhase::WaitingPatch);
        const auto lateness=snapshot.scene.applied_phase-snapshot.scene.due_phase;
        const auto speed=tempo::Position{0,0,bpm}.unitsPerFrame();
        assert(lateness<speed*amy::kBlockFrames);
        runtime.loadPendingPatch();
        advance();
        assert(snapshot.scene.phase==scene::RecallPhase::Confirmed);
        std::printf("PASS PATCH Next Bar at %u BPM, lateness %.2f frames\n",unsigned(bpm),double(lateness)/speed);
    }
    std::printf("PASS AMY scene audio owner recall, Next Bar lateness %.2f frames\n",
                double(late)/units_per_frame);
    double max_load_us=0,total_load_us=0;
    unsigned load_attempts=0,over_budget=0,max_allocs=0;
    uint64_t max_allocated=0,max_released=0;
    size_t stable_bytes[2]{};
    uint32_t stable_live[2]{};
    const auto usb=[&](uint8_t status,uint8_t id,uint8_t value) {
        const uint8_t packet[]={uint8_t(status>>4),status,id,value};
        assert(runtime.usbInput().transfer(packet,sizeof(packet),block*amy::kBlockFrames*1000/amy::kSampleRateHz));
    };
    const auto setArpEnabled=[&](bool enabled) {
        amy::Command command{amy::CommandType::Arp,uint16_t(arp::Action::Enable),uint8_t(enabled),0,
                            commands.nextToken(),block*amy::kBlockFrames*1000/amy::kSampleRateHz};
        command.version=arp::kProtocolVersion;
        assert(commands.push(command));
    };
    for(unsigned i=0;i<1000;++i) {
        const auto kind=i%4;
        const auto domains=kind==0?scene::kAll:kind==1?uint8_t(scene::Patch|scene::Fx):
                           kind==2?uint8_t(scene::Pattern):uint8_t(scene::Fx);
        const bool swaps=domains&scene::Patch;
        if(swaps) {
            assert(commands.push({amy::CommandType::Harmony,0,0,0,harmony::pack(harmony::Config{})}));
            setArpEnabled(false);
            usb(0xb0,64,127);
            for(uint8_t note=60;note<68;++note)usb(0x90,note,100);
            advance();
            assert(snapshot.active_notes==8);
            usb(0x80,60,0);
            advance();
            assert(snapshot.active_notes==8);
        }
        const bool concurrent_transport=kind==3 && i%16==3;
        if(concurrent_transport) {
            seq::Request play;
            play.action=seq::Action::Play;
            play.token=commands.nextToken();
            play.submitted_ms=block*amy::kBlockFrames*1000/amy::kSampleRateHz;
            assert(commands.pushSequencer(play));
            setArpEnabled(true);
            usb(0x90,72,100);
            advance();
            assert(snapshot.sequencer.playing && snapshot.arp.held_count==1);
        }
        request=makeRequest(domains);
        request.configuration.patch_id=snapshot.patch_id==4?128:4;
        request.configuration.pattern_slot=5;
        request.pattern.state=seq::SlotState::Valid;
        request.pattern.sequence=9;
        request.pattern.settings=request.expected_pattern;
        request.discard_pattern=true;
        backend.publish(snapshot,true,0,0);
        const auto token=backend.submitScene(request);
        assert(token && !backend.submitScene(request));
        advance();
        const bool fail=swaps && i%64==0;
        const bool panic=swaps && i%64==1;
        if(swaps) {
            assert(snapshot.scene.phase==scene::RecallPhase::WaitingPatch && !snapshot.active_notes);
            assert(snapshot.scene.generation==request.generation+1);
            if(fail)probe_fail_next(1);
            if(panic)probe_on_allocation(1,[] { runtime.commands().panic(); });
            const auto calls=probe_calls();
            const auto allocated=probe_allocated_bytes();
            const auto released=probe_released_bytes();
            const auto start=Clock::now();
            runtime.loadPendingPatch();
            const auto elapsed=Micros(Clock::now()-start).count();
            ++load_attempts;
            total_load_us+=elapsed;
            max_load_us=std::max(max_load_us,elapsed);
            over_budget+=elapsed>amy::kBlockFrames*1e6/amy::kSampleRateHz;
            max_allocs=std::max(max_allocs,probe_calls()-calls);
            max_allocated=std::max(max_allocated,probe_allocated_bytes()-allocated);
            max_released=std::max(max_released,probe_released_bytes()-released);
            advance();
        }
        backend.publish(snapshot,true,0,0);
        assert(snapshot.scene.token==token && backend.sceneStatus().token==token);
        assert(backend.sceneStatus().phase==snapshot.scene.phase);
        if(concurrent_transport) {
            assert(snapshot.sequencer.playing && snapshot.arp.held_count==1);
            commands.panic();
            advance();
            assert(!snapshot.arp.held_count && !snapshot.sequencer.playing);
        }
        assert(!snapshot.active_notes);
        if(fail || panic) {
            assert(snapshot.scene.phase==scene::RecallPhase::Failed && !snapshot.scene.applied_domains);
            assert(snapshot.patch_state==amy::PatchState::Failed);
            retryPatch();
        } else {
            assert(snapshot.scene.phase==scene::RecallPhase::Confirmed && snapshot.scene.applied_domains==domains);
            assert(snapshot.scene.generation==request.generation+(swaps?2:0));
        }
        const unsigned topology=snapshot.patch_id==128;
        if(!stable_bytes[topology]) {
            stable_bytes[topology]=probe_bytes();
            stable_live[topology]=probe_live();
        }
        assert(stable_bytes[topology]==probe_bytes() && stable_live[topology]==probe_live());
    }
    std::printf("PASS 1000 scene recalls, %u measured load attempts, stable tracked heap by topology\n",load_attempts);
    std::printf("MEASURE host patch avg/max %.1f/%.1f us over_block_budget=%u max_allocations=%u "
                "max_allocated=%llu max_released=%llu peak_tracked=%zu bytes\n",
                total_load_us/load_attempts,max_load_us,over_budget,max_allocs,
                static_cast<unsigned long long>(max_allocated),static_cast<unsigned long long>(max_released),
                probe_peak_bytes());
    std::printf("MEASURE host runtime=%zu snapshot=%zu scene mailbox=%zu bytes\n",
                sizeof(runtime),sizeof(snapshot),sizeof(commands.scenes));
    std::printf("MEASURE host owner apply avg/max %.1f/%.1f us, render %.1f/%.1f us (%u blocks)\n",
                apply_us/measured_blocks,max_apply_us,render_us/measured_blocks,max_render_us,measured_blocks);
    runtime.end();
    assert(probe_bytes()==initial_bytes && probe_live()==initial_live);
}
