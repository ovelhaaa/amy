#include "amy_runtime.h"
#include <cassert>
#include <cstdio>
#include <cstring>

extern "C" void probe_realtime(bool);

using namespace smk::p4;

int main() {
    static amy::Runtime runtime;
    assert(runtime.begin());
    auto& commands=runtime.commands();
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
        request.expected_pattern={snapshot.sequencer_pattern,snapshot.sequencer.division,
                                  snapshot.sequencer.swing,snapshot.sequencer.transpose,
                                  snapshot.sequencer.quantize};
        request.expected_pattern_slot=snapshot.sequencer.active_slot;
        request.apply_tempo=true;
        request.slot=3;
        return request;
    };
    unsigned block=0;
    auto advance=[&]() {
        probe_realtime(true);
        runtime.apply(block++*amy::kBlockFrames*1000/amy::kSampleRateHz);
        assert(runtime.render());
        probe_realtime(false);
    };
    auto request=makeRequest(scene::Fx|scene::Harmony|scene::Arp|scene::Tempo);
    assert(commands.scenes.push(request));
    assert(!commands.scenes.push(request));
    advance();
    const auto& snapshot=runtime.snapshot();
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
    assert(commands.scenes.push(request));
    advance();
    assert(snapshot.scene.error==scene::RecallError::UnsupportedDomains);
    assert(snapshot.fx.config.delay_mix==0.35f);

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
    std::printf("PASS AMY scene audio owner recall, Next Bar lateness %.2f frames\n",
                double(late)/units_per_frame);
    std::printf("MEASURE host runtime=%zu snapshot=%zu scene mailbox=%zu bytes\n",
                sizeof(runtime),sizeof(snapshot),sizeof(commands.scenes));
}
