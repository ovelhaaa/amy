#include "amy_runtime.h"
#include "performance_factory_routes.h"
#include <initializer_list>
#include <cstring>
#include <cmath>
#ifdef SMKP4_HARMONY
#ifdef ESP_PLATFORM
#include "esp_timer.h"
#else
#include <chrono>
#endif
#endif
extern "C" {
#include "amy.h"
void amy_platform_init() {}
void amy_platform_deinit() {}
void delay_ms(uint32_t) {}
void amy_update_tasks() {}
int16_t* amy_render_audio() { return amy_simple_fill_buffer(); }
size_t amy_i2s_write(const uint8_t*,size_t bytes) { return bytes; }
void alloc_chorus_delay_lines(uint8_t bus);
void dealloc_chorus_delay_lines(uint8_t bus);
bool alloc_echo_delay_lines(uint8_t bus, uint32_t max_delay_samples);
void dealloc_echo_delay_lines(uint8_t bus);
bool alloc_reverb_delay_lines(uint8_t bus);
void dealloc_reverb_delay_lines(uint8_t bus);
void config_eq(uint8_t bus, SAMPLE eq_l, SAMPLE eq_m, SAMPLE eq_h);
}
namespace smk::p4::amy {
#ifdef SMKP4_HARMONY
static uint32_t musicalTimeUs() {
#ifdef ESP_PLATFORM
    return uint32_t(esp_timer_get_time());
#else
    return uint32_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
#endif
}
struct AdmissionTimer {
    uint32_t start=musicalTimeUs();uint32_t& maximum;
    explicit AdmissionTimer(uint32_t& value):maximum(value){}
    ~AdmissionTimer(){const uint32_t elapsed=musicalTimeUs()-start;if(elapsed>maximum)maximum=elapsed;}
};
#endif
static_assert(AMY_SAMPLE_RATE==kSampleRateHz && AMY_BLOCK_SIZE==kBlockFrames && AMY_NCHANS==2);
size_t Runtime::bootFreeInternalBytes() {
    constexpr unsigned blocks=(kDeltaReserve+kDeltaBlockSize-1)/kDeltaBlockSize;
    const size_t historical=kBootBaseFreeInternalBytes+(blocks-1)*kDeltaBlockSize*sizeof(struct delta);
    const size_t full_engine=kLibraryEngineHeapBudgetBytes+kPostBootFreeInternalBytes;
    return kPatchCount>2 && full_engine>historical?full_engine:historical;
}
bool Runtime::hasBootMemory(size_t free_internal_bytes,size_t largest_internal_bytes) {
    return free_internal_bytes>=bootFreeInternalBytes() && largest_internal_bytes>=kBootLargestInternalBytes;
}
bool Runtime::begin(uint32_t sample_rate_hz) {
    if(started_ || sample_rate_hz!=kSampleRateHz) return false;
    state_=Snapshot{}; bank_=0; gain_q16_=0;
#ifdef SMKP4_SOUND_EDITOR
    arp_=arp::Engine{};tempo_=tempo::Clock{};arp_identity_=0;
    sequencer_=seq::Engine{};sequencer_group_=kVoices;
    echo_time_ms_=350.0f;echo_mix_=0.0f;echo_feedback_=0.0f;
    last_sync_ms_=0.0f;echo_sync_=false;
#endif
    for(auto& v:voices_) v=Voice{};
    for(auto& g:groups_) g=Group{};
    commands_.panic(OutputReason::Boot); state_.generation=commands_.generation();
    publishOutput(OutputReason::Boot);
    auto config=amy_default_config();
    config.features.default_synths=config.features.startup_bleep=0;
    config.features.reverb=config.features.chorus=config.features.echo=1;
    config.features.partials=config.features.custom=0;
    config.platform.multicore=config.platform.multithread=0;
    config.ks_oscs=0; config.max_oscs=kOscillators;
    config.max_voices=kAmyVoices; config.max_synths=kSynthCount;
    config.max_memory_patches=1; config.max_sequencer_tags=1;
#ifdef ESP_PLATFORM
    config.ram_caps_synth=config.ram_caps_events=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
    config.ram_caps_block=config.ram_caps_fbl=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
    config.ram_caps_delay=config.ram_caps_sample=config.ram_caps_sysex=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
#endif
    amy_start(config); started_=true;
    // Gate B: Safe FX preallocation outside the audio render path
    if(!ensure_osc_allocd(CHORUS_MOD_SOURCE + 0, nullptr)) { end(); return false; }
    alloc_chorus_delay_lines(0);
    if(!alloc_echo_delay_lines(0, 65536)) { end(); return false; }
    if(!alloc_reverb_delay_lines(0)) { end(); return false; }
    if(!amy_global.bus[0]->chorus.chorus_delay_lines[0] ||
       !amy_global.bus[0]->chorus.chorus_delay_lines[1] ||
       !amy_global.bus[0]->chorus.delay_mod ||
       !amy_global.bus[0]->echo.echo_delay_lines[0] ||
       !amy_global.bus[0]->echo.echo_delay_lines[1] ||
       !amy_global.bus[0]->reverb.rev ||
       !amy_global.bus[0]->reverb.rev->delay_1) {
        end(); return false;
    }
    state_.fx.applied=state_.fx.config;
    fx::applyBusFx(state_.fx.config);
    for(size_t i=0;i<kAmyVoices;++i) {
        const auto synth_id=static_cast<uint8_t>(i+1);
        const auto patch_id=kPatches[i/kVoices].id;
        auto e=amy_default_event(); e.synth=synth_id;
        e.patch_number=patch_id; e.num_voices=1; e.bus=0; e.synth_delay_ms=0;
        amy_add_event(&e);
        uint16_t voice;
        if(instrument_get_patch_number(synth_id)!=patch_id || instrument_get_num_voices(synth_id,&voice)!=1) { end(); return false; }
        const int oscs=instrument_get_oscs_per_voice(e.synth);
        unsigned count=0;
        for(unsigned o=0;o<kOscillators;++o) if(osc_to_voice[o]==voice) ++count;
        if(oscs!=static_cast<int>(kOscillatorsPerVoice) || count!=static_cast<unsigned>(oscs)) { end(); return false; }
    }
    usb_.disconnect(); usb_generation_=usb_.commands.generation();
    vibrato_frame_=0;
    // Materialize all patch deltas and warm DSP before the timed audio service.
    for(unsigned i=0;i<2;++i) amy_simple_fill_buffer();
    silence(); amy_execute_deltas();
    // Exercise checked pool growth only at boot. Hold the entire bounded
    // service demand simultaneously, then return it without changing DSP state.
    struct delta* reserved=nullptr;
    for(unsigned i=0;i<kDeltaReserve;++i) {
        struct delta* d=nullptr;
        if(delta_get_checked(nullptr,&d)!=AMY_RESOURCE_OK) {
            delta_release_list(reserved); state_.oom_count=amy_get_oom_count();
            end(); return false;
        }
        d->next=reserved; reserved=d;
    }
    delta_release_list(reserved);
    state_.oom_count=amy_get_oom_count();
    if(state_.oom_count) { end(); return false; }
    #ifdef SMKP4_SOUND_EDITOR
    captureEdit();
#endif
    state_.ready=true; publishOutput(OutputReason::Ready); return true;
}
void Runtime::end() {
    if(started_) { silence(); amy_stop(); started_=false; }
    state_.ready=false; state_.active_notes=0;
    for(auto& v:voices_) v=Voice{};
    for(auto& g:groups_) g=Group{};
    commands_.panic();
}
void Runtime::silence() {
#ifdef SMKP4_SOUND_EDITOR
    arp_.stop();arp_identity_=0;publishArp();
    sequencer_.resetMidi();
    std::memset(usb_note_routes_,0,sizeof(usb_note_routes_));
    sequencer_device_generation_=usb_.deviceGeneration();
    stopSequencer();
#endif
    amy_deltas_reset();
    for(size_t i=0;i<kAmyVoices;++i) {
#ifdef SMKP4_UNIFIED_ENGINE
        if(instrument_get_num_voices(i+1,nullptr)<=0) continue;
#endif
        auto e=amy_default_event(); e.synth=static_cast<uint8_t>(i+1); e.pedal=0; e.velocity=0;
        amy_add_event(&e);
    }
    all_notes_off();
    std::memset(sustain_,0,sizeof(sustain_));
#ifdef SMKP4_SOUND_EDITOR
    state_.sustain_active=false;
#endif
    std::memset(bend_,0,sizeof(bend_)); std::memset(modulation_,0,sizeof(modulation_));
    for(auto& v:voices_) v=Voice{};
    for(auto& g:groups_) g=Group{};
    state_.active_notes=0;state_.last_count=0;state_.last_admitted=false;state_.last_result=0;
#ifdef SMKP4_SOUND_EDITOR
    if(started_ && state_.ready && state_.patch_state==PatchState::Ready && state_.edit.patch==state_.patch_id) updateModWheelFilter();
#endif
}
void Runtime::note(unsigned slot,uint8_t velocity) {
    const auto& v=voices_[slot];
    auto e=amy_default_event(); e.synth=bank_*kVoices+slot+1;
    e.midi_note=v.pitch; e.velocity=velocity/127.0f; amy_add_event(&e);
#ifdef SMKP4_SOUND_EDITOR
    if(velocity && state_.edit.family==0 && edit_count_>=8) {
        const float vel=velocity/127.0f;
        for(unsigned op=0;op<6;++op) {
            const unsigned o=edit_map_[slot][edit_ops_[op]];
            if(o<kOscillators && synth[o]) synth[o]->velocity=vel;
        }
    }
#endif
    if(velocity && v.usb) tune(slot);
}
void Runtime::tune(unsigned slot) {
    const auto& v=voices_[slot];
    auto e=amy_default_event(); e.synth=bank_*kVoices+slot+1;
    // AMY pitch_bend is global. Per-voice fractional note updates preserve
    // channel and Touch isolation, without retriggering velocity/envelopes.
    constexpr float tau=6.28318530718f;
    float vibrato=0.5f*(modulation_[v.channel]/127.0f)*
        std::sin(tau*5.0f*vibrato_frame_/kSampleRateHz);
#ifdef SMKP4_SOUND_EDITOR
    if(!state_.edit.compare && (state_.edit.available&edit::bit(edit::ModEnable))) {
        const bool en=std::lround(state_.edit.applied[edit::ModEnable])==1;
        const int src=int(std::lround(state_.edit.applied[edit::ModSource]));
        const int dst=int(std::lround(state_.edit.applied[edit::ModDest]));
        if(!en || src!=1 || dst!=1) vibrato=0.0f;
        else {
            const float amt=state_.edit.applied[edit::ModAmount]/100.0f;
            const float rate=((state_.edit.mask&edit::bit(edit::LfoRate)) && (state_.edit.available&edit::bit(edit::LfoRate)))?
                state_.edit.applied[edit::LfoRate]:5.0f;
            const float s=std::sin(tau*rate*vibrato_frame_/kSampleRateHz);
            const int pol=int(std::lround(state_.edit.applied[edit::ModPolarity]));
            const float shape=(pol==0)?(0.5f*(1.0f+s)):(pol==2)?(-0.5f*(1.0f+s)):s;
            vibrato=(modulation_[v.channel]/127.0f)*(amt*shape);
        }
    }
#endif
    e.midi_note=v.pitch+bend_[v.channel]*(2.0f/8192.0f)+vibrato;
    amy_add_event(&e);
#ifdef SMKP4_SOUND_EDITOR
    updateModWheelFilter();
#endif
}
void Runtime::release(unsigned slot) {
    if(!voices_[slot].used) return;
    note(slot,0);
    auto& g=groups_[voices_[slot].group];g.live&=uint16_t(~(1u<<slot));
    if(!g.live) {
#ifdef SMKP4_SOUND_EDITOR
        if(g.origin==NoteOrigin::Arpeggiator && g.identity==arp_identity_) {
            arp_identity_=0;arp_.sounding(false);
        }
#endif
        g=Group{};
    }
    voices_[slot]=Voice{}; --state_.active_notes;
}
unsigned Runtime::findGroup(NoteOrigin origin,uint8_t channel,uint8_t input) const {
    for(unsigned i=0;i<kVoices;++i)if(groups_[i].used && groups_[i].origin==origin &&
        groups_[i].channel==channel && groups_[i].input==input)return i;
    return kVoices;
}
void Runtime::releaseGroup(unsigned group) {
    for(unsigned i=0;i<kVoices;++i)if(voices_[i].used && voices_[i].group==group)release(i);
}
unsigned Runtime::startGroup(NoteOrigin origin,const Command& c,uint32_t now_ms) {
    const bool usb=origin==NoteOrigin::UsbDirect || origin==NoteOrigin::Arpeggiator;
#ifdef SMKP4_HARMONY
    AdmissionTimer admission(state_.admission_max_us);
    const uint32_t transform_start=musicalTimeUs();
#endif
    auto config=state_.harmony;
#ifdef SMKP4_SOUND_EDITOR
    if(origin==NoteOrigin::Sequencer) {
        config.scale_on=sequencer_.status().quantize;
        config.chord_on=false;
    } else
#endif
    {
        const bool eligible=usb?c.channel==config.channel:config.touch;
        if(!eligible)config.scale_on=config.chord_on=false;
    }
    const auto notes=harmony::transform(uint8_t(c.id),config);
#ifdef SMKP4_HARMONY
    const uint32_t transform_us=musicalTimeUs()-transform_start;
    if(transform_us>state_.transform_max_us)state_.transform_max_us=transform_us;
#endif
    state_.last_input=uint8_t(c.id);state_.last_admitted=false;state_.last_count=0;
    if(notes.error!=harmony::Error::None){state_.last_result=notes.error==harmony::Error::Filtered?2:4;if(state_.last_result==4)++state_.harmony_errors;return kVoices;}
    unsigned group=kVoices,free=0;
    for(unsigned i=0;i<kVoices;++i){if(!voices_[i].used)++free;if(!groups_[i].used && group==kVoices)group=i;}
    if(free<notes.count || group==kVoices || next_group_==UINT32_MAX) {
        state_.last_result=3;
        if(notes.count>1)++state_.groups_rejected;else ++state_.polyphony_rejected;
        return kVoices;
    }
    auto& g=groups_[group];g=Group{};
    g.used=true;g.origin=origin;g.held=true;g.channel=usb?c.channel:0;g.input=uint8_t(c.id);
    g.token=c.token;g.identity=++next_group_;g.generation=usb?usb_generation_:state_.generation;
    g.started_ms=now_ms;g.configuration=config;g.count=notes.count;
    unsigned n=0;
    // Owner is sole allocator. Reserve every slot before issuing any AMY On.
    for(unsigned i=0;i<kVoices && n<notes.count;++i)if(!voices_[i].used) {
        g.notes[n]=notes.notes[n];g.slots[n]=uint8_t(i);g.live|=uint16_t(1u<<i);
        auto& v=voices_[i];v.used=v.held=true;v.usb=usb;v.pitch=notes.notes[n];v.channel=g.channel;
        v.input=g.input;v.group=uint8_t(group);v.token=c.token;v.started_ms=now_ms;++n;
    }
    state_.active_notes+=notes.count;
    for(unsigned i=0;i<notes.count;++i){note(g.slots[i],c.value);state_.last_notes[i]=notes.notes[i];}
    state_.last_result=1;state_.last_group=g.identity;state_.last_count=notes.count;state_.last_admitted=true;++state_.groups_admitted;
    return group;
}
void Runtime::cancelUsb() {
#ifdef SMKP4_SOUND_EDITOR
    cancelScene();
    arp_.stop();arp_identity_=0;publishArp();
    std::memset(usb_note_routes_,0,sizeof(usb_note_routes_));
    sequencer_.resetMidi();
    const auto device_generation=usb_.deviceGeneration();
    if(sequencer_device_generation_!=device_generation) {
        sequencer_device_generation_=device_generation;
        releaseSequencer();
        sequencer_.stop();
    }
    publishSequencer();
#endif
    for(unsigned i=0;i<kVoices;++i) if(voices_[i].used && voices_[i].usb) release(i);
    std::memset(sustain_,0,sizeof(sustain_));
    std::memset(bend_,0,sizeof(bend_)); std::memset(modulation_,0,sizeof(modulation_));
#ifdef SMKP4_SOUND_EDITOR
    if(started_ && state_.ready && state_.patch_state==PatchState::Ready && state_.edit.patch==state_.patch_id) updateModWheelFilter();
#endif
}
void Runtime::applyUsb(uint32_t now_ms) {
    auto& q=usb_.commands;
#ifdef SMKP4_SOUND_EDITOR
    bool pattern_changed=false, midi_changed=false;
    const bool smk25=usb_.isSmk25();
    if(sequencer_.status().midi.smk_connected!=smk25) {
        sequencer_.setSmkConnected(smk25);
        midi_changed=true;
    }
#endif
    if(q.generation()!=usb_generation_) { cancelUsb(); usb_generation_=q.generation(); }
    Command c;
    unsigned applied=0;
    for(unsigned scanned=0;scanned<kUsbCommandCapacity && applied<kCommandCapacity && q.pop(c);++scanned) {
        if(c.generation!=usb_generation_ || c.generation!=q.generation()) continue;
        ++applied;
        const uint32_t latency=eventAgeMs(now_ms,c.timestamp_ms);
        if(latency>state_.command_latency_max_ms) state_.command_latency_max_ms=latency;
        if(c.channel>=16) { ++state_.invalid_commands; continue; }
#ifdef SMKP4_SOUND_EDITOR
        if(c.type==CommandType::Arp) { applyArp(c,true,now_ms);continue; }
#endif
        if(c.type==CommandType::NoteOn || c.type==CommandType::NoteOff) {
            if(c.id>=128 || c.value>127) { ++state_.invalid_commands; continue; }
            const bool off=c.type==CommandType::NoteOff || !c.value;
#ifdef SMKP4_SOUND_EDITOR
            auto& routes=usb_note_routes_[c.channel][c.id/4];
            const unsigned shift=2*(c.id%4);
            const auto previous=UsbNoteRoute((routes>>shift)&3);
            auto route=previous;
            if(off) {
                routes=uint8_t(routes&~(3u<<shift));
                if(route==UsbNoteRoute::None || route==UsbNoteRoute::Captured) continue;
            } else {
                if(previous!=UsbNoteRoute::None) continue;
                if(latency>=kLeaseMs) {
                    sequencer_.countRoutingError();
                    midi_changed=true;
                    continue;
                }
                bool changed=false;
                const bool consumed=sequencer_.midiOn(c.channel,uint8_t(c.id),c.value,smk25,changed);
                pattern_changed|=changed;
                midi_changed=true;
                route=consumed?UsbNoteRoute::Captured:
                    arp_.status().config.enabled?UsbNoteRoute::Arp:UsbNoteRoute::Direct;
                routes=uint8_t(routes|(unsigned(route)<<shift));
                if(consumed) continue;
            }
            if(route==UsbNoteRoute::Arp) {
                const arp::Input input{uint8_t(c.id),c.value,c.channel,c.generation};
                if(off) arp_.off(input);
                else arp_.on(input);
                continue;
            }
#endif
            const unsigned found=findGroup(NoteOrigin::UsbDirect,c.channel,uint8_t(c.id));
            if(off) {
                if(found<kVoices) {
                    auto& g=groups_[found];g.held=false;g.sustained=sustain_[c.channel];
                    for(unsigned i=0;i<kVoices;++i)if(voices_[i].used && voices_[i].group==found)voices_[i].held=false;
                    if(!sustain_[c.channel])releaseGroup(found);
                }
            } else {
                if(latency>=kLeaseMs)continue;
                if(found<kVoices && groups_[found].held)continue;
                if(found<kVoices)releaseGroup(found);
                startGroup(NoteOrigin::UsbDirect,c,now_ms);
            }
        } else if(c.type==CommandType::Control && c.value<=127) {
            if(c.id==64) {
                sustain_[c.channel]=c.value>=64;
                if(!sustain_[c.channel]) for(unsigned i=0;i<kVoices;++i) {
                    const auto& v=voices_[i];
                    if(v.used && v.usb && v.channel==c.channel && !v.held) release(i);
                }
            } else if(c.id==120 || c.id==123) {
#ifdef SMKP4_SOUND_EDITOR
                arp_.clearChannel(c.channel);
                std::memset(usb_note_routes_[c.channel],0,sizeof(usb_note_routes_[c.channel]));
#endif
                // Safety policy: both release this USB channel immediately.
                sustain_[c.channel]=false;
                for(unsigned i=0;i<kVoices;++i) if(voices_[i].used && voices_[i].usb && voices_[i].channel==c.channel) release(i);
            } else if(c.id==1) {
                modulation_[c.channel]=c.value;
                for(unsigned i=0;i<kVoices;++i) if(voices_[i].used && voices_[i].usb && voices_[i].channel==c.channel) tune(i);
#ifdef SMKP4_SOUND_EDITOR
                updateModWheelFilter();
#endif
            }
        } else if(c.type==CommandType::Bend && c.id<16384) {
            bend_[c.channel]=static_cast<int16_t>(int(c.id)-8192);
            for(unsigned i=0;i<kVoices;++i) if(voices_[i].used && voices_[i].usb && voices_[i].channel==c.channel) tune(i);
        } else ++state_.invalid_commands;
    }
#ifdef SMKP4_SOUND_EDITOR
    if(pattern_changed) state_.sequencer_pattern=sequencer_.pattern();
    if(midi_changed) {
        sequencer_.updateLibraryStatus();
        publishSequencer();
    }
#endif
    if(q.generation()!=usb_generation_) { cancelUsb(); usb_generation_=q.generation(); }
}
#ifdef SMKP4_SOUND_EDITOR
void Runtime::releaseArp() {
    if(!arp_identity_) return;
    for(unsigned i=0;i<kVoices;++i)
        if(groups_[i].used && groups_[i].origin==NoteOrigin::Arpeggiator &&
           groups_[i].identity==arp_identity_) releaseGroup(i);
    arp_identity_=0;arp_.sounding(false);
}

void Runtime::applyArp(const Command& c,bool usb,uint32_t now_ms) {
    const auto action=static_cast<arp::Action>(c.id);
    const auto current=usb?usb_.commands.generation():commands_.generation();
    auto error=arp::Error::None;
    if(c.generation!=current || eventAgeMs(now_ms,c.timestamp_ms)>=kLeaseMs) error=arp::Error::Stale;
    else if(!c.token || c.version!=arp::kProtocolVersion ||
            !(action==arp::Action::Bpm?tempo_.setBpm(c.value):arp_.configure(action,c.value))) error=arp::Error::Invalid;
    else if(action==arp::Action::Bpm) resolveDelay();
    else if(action==arp::Action::ClearLatch) releaseArp();
    else if(action==arp::Action::Enable || action==arp::Action::Toggle) {
        if(state_.arp.config.enabled!=arp_.status().config.enabled) {
            for(unsigned i=0;i<kVoices;++i) if(voices_[i].used && voices_[i].usb) release(i);
            arp_identity_=0;
        }
    } else if(action==arp::Action::Stop) {
        cancelScene();
        if(!usb) {
            usb_.commands.panic();
            cancelUsb();
            usb_generation_=usb_.commands.generation();
        }
    }
    arp_.acknowledge(action,c.value,c.token,c.generation,usb,error,tempo_.position());
    publishArp();
}

void Runtime::serviceArp() {
#ifdef SMKP4_HARMONY
    AdmissionTimer timing(state_.arp_service_max_us);
#endif
    if(usb_.commands.generation()!=usb_generation_) {
        cancelUsb();
        usb_generation_=usb_.commands.generation();
    }
    if(commands_.generation()!=state_.generation) {
        releaseArp();arp_.stop();publishArp();return;
    }
    arp_.prepare();
    if(!arp_.status().length || state_.blocks-arp_started_block_>=arp_gate_blocks_) releaseArp();
    arp::Input input;
    uint32_t gate_frames=0;
    if(arp_.next(input,gate_frames,tempo_.position())) {
        releaseArp();
        if(input.generation==usb_generation_ && input.generation==usb_.commands.generation()) {
            Command c{};
            c.id=input.pitch;c.value=input.velocity;c.channel=input.channel;
            const unsigned group=startGroup(NoteOrigin::Arpeggiator,c,0);
            if(group<kVoices) {
                arp_identity_=groups_[group].identity;
                groups_[group].token=arp_identity_;
                arp_started_block_=state_.blocks;
                arp_gate_blocks_=gate_frames?(gate_frames+kBlockFrames-1)/kBlockFrames:1;
                arp_.sounding(true);
            }
        }
    }
    publishArp();
}

void Runtime::releaseSequencer() {
    if(sequencer_group_<kVoices) releaseGroup(sequencer_group_);
    sequencer_group_=kVoices;
}

void Runtime::stopSequencer() {
    sequencer_.stop();
    sequencer_group_=kVoices; // Global silence also clears the voice and group tables.
    publishSequencer();
}

void Runtime::applySequencer(const seq::Request& request,uint32_t now_ms) {
    using seq::Action;
    bool accepted=request.token && request.version==seq::kProtocolVersion &&
        request.generation==state_.generation && request.generation==commands_.generation() &&
        (state_.patch_state==PatchState::Ready || request.action==Action::Stop) &&
        (!request.timed || request.action==Action::Stop ||
         eventAgeMs(now_ms,request.submitted_ms)<seq::kConfirmationTimeoutMs);
    if(accepted) {
        switch(request.action) {
        case Action::LoadSlot: {
            bool release=false;
            accepted=request.value>=0 && request.value<=int(seq::SwitchMode::NextBar) &&
                sequencer_.load(request.index,request.settings(),seq::SwitchMode(request.value),
                                request.discard,request.token,tempo_.position(),release);
            if(release) releaseSequencer();
            break;
        }
        case Action::MarkSlotSaved:
            accepted=request.index<seq::kPatternSlots && seq::Engine::valid(request.settings());
            if(accepted) sequencer_.markSaved(request.index,request.settings());
            break;
        case Action::DetachSlot:
            accepted=request.index<seq::kPatternSlots;
            if(accepted) sequencer_.detachSlot(request.index);
            break;
        case Action::CancelSwitch: sequencer_.cancelSwitch(); break;
        case Action::Play:
            accepted=sequencer_.play(tempo_.position());
            break;
        case Action::Stop:
            cancelScene();
            sequencer_.stop();
            releaseSequencer();
            break;
        case Action::Restart:
            if(sequencer_.status().playing && sequencer_.restart(tempo_.position())) releaseSequencer();
            break;
        case Action::SetStep:
            accepted=sequencer_.setStep(request.index,request.step);
            break;
        case Action::SetLength:
            accepted=sequencer_.setLength(request.value);
            break;
        case Action::SetDivision:
            accepted=request.value>=0 && request.value<int(tempo::Division::Count) &&
                sequencer_.setDivision(tempo::Division(request.value));
            break;
        case Action::SetPattern:
            accepted=sequencer_.setPattern(request.pattern);
            break;
        case Action::SetEnabled:
            accepted=request.value==0 || request.value==1;
            if(accepted && sequencer_.setEnabled(request.value!=0)) releaseSequencer();
            break;
        case Action::SetSwing:
            accepted=sequencer_.setSwing(request.value);
            break;
        case Action::SetTranspose:
            accepted=sequencer_.setTranspose(request.value);
            break;
        case Action::SetQuantize:
            accepted=request.value==0 || request.value==1;
            if(accepted) sequencer_.setQuantize(request.value!=0);
            break;
        case Action::SelectStep: accepted=sequencer_.select(request.value); break;
        case Action::MoveStep: accepted=sequencer_.moveSelection(request.value); break;
        case Action::SetInputMode:
            accepted=request.index<seq::kSteps && sequencer_.setInputMode(request.value);
            if(accepted) sequencer_.select(request.index);
            break;
        case Action::SetInputChannel: accepted=sequencer_.setMidiChannel(request.value,false); break;
        case Action::SetLiveChannel: accepted=sequencer_.setMidiChannel(request.value,true); break;
        case Action::SetRecordGate: accepted=sequencer_.setRecordGate(request.value); break;
        case Action::SetReference: accepted=sequencer_.setReference(request.value); break;
        case Action::SetLive:
        case Action::SetThru:
        case Action::SetSmkOnly:
            accepted=request.value==0 || request.value==1;
            if(accepted && request.action==Action::SetLive) sequencer_.setLive(request.value!=0);
            if(accepted && request.action==Action::SetThru) sequencer_.setThru(request.value!=0);
            if(accepted && request.action==Action::SetSmkOnly) sequencer_.setSmkOnly(request.value!=0);
            break;
        case Action::Rest: accepted=sequencer_.rest(); break;
        case Action::SetStepNote:
        case Action::SetStepVelocity:
        case Action::SetStepGate:
        case Action::SetStepEnabled:
        case Action::ToggleStepEnabled:
        case Action::ShiftStepNote: {
            accepted=false;
            if(request.index>=seq::kSteps) break;
            auto step=sequencer_.pattern().steps[request.index];
            const int value=request.value;
            if(request.action==Action::SetStepNote && value>=0 && value<=127) {
                step.note=uint8_t(value);
                step.enabled=true;
            } else if(request.action==Action::SetStepVelocity && value>=1 && value<=127)
                step.velocity=uint8_t(value);
            else if(request.action==Action::SetStepGate && value>=1 && value<=100)
                step.gate=uint8_t(value);
            else if(request.action==Action::SetStepEnabled && (value==0 || value==1))
                step.enabled=value!=0;
            else if(request.action==Action::ToggleStepEnabled)
                step.enabled=!step.enabled;
            else if(request.action==Action::ShiftStepNote)
                step.note=uint8_t(std::clamp(int(step.note)+value,0,127));
            else break;
            accepted=sequencer_.setStep(request.index,step);
            if(accepted && request.select) sequencer_.select(request.index);
            break;
        }
        default:
            accepted=false;
            break;
        }
    }
    if(accepted) {
        switch(request.action) {
        case Action::LoadSlot: case Action::SetPattern: case Action::SetLength: case Action::SetStep:
        case Action::Rest: case Action::SetStepNote: case Action::SetStepVelocity:
        case Action::SetStepGate: case Action::SetStepEnabled: case Action::ShiftStepNote:
        case Action::ToggleStepEnabled:
            state_.sequencer_pattern=sequencer_.pattern();
            break;
        default: break;
        }
    }
    sequencer_.updateLibraryStatus();
    sequencer_.acknowledge(request.token,accepted,request.action==Action::Stop);
    publishSequencer();
}

void Runtime::serviceSequencer() {
    const auto previous_slot=sequencer_.status().pending_slot;
    const auto events=sequencer_.update(tempo_.position());
    if(previous_slot!=seq::kNoSlot && sequencer_.status().pending_slot==seq::kNoSlot) {
        state_.sequencer_pattern=sequencer_.pattern();
        sequencer_.updateLibraryStatus();
    }
    if(events.release) releaseSequencer();
    if(events.velocity) {
        Command note{};
        note.id=events.note;
        note.value=events.velocity;
        const auto dropped=state_.polyphony_rejected;
        sequencer_group_=startGroup(NoteOrigin::Sequencer,note,0);
        if(dropped!=state_.polyphony_rejected) sequencer_.countVoiceDrop();
    }
    publishSequencer();
}
#endif
void Runtime::applyFx(uint32_t now_ms) {
    bool updated = false;
    for(unsigned i=0;i<fx::Mailbox::capacity;++i) {
        const auto* entry=commands_.fx.front();
        // Observe publication before sampling the generation. A request from
        // after a concurrent Panic must survive until the owner catches up.
        if(!entry || commands_.generation()!=state_.generation) break;
        const auto req=*entry;
        commands_.fx.consume();
        if(req.preset_recall && eventAgeMs(now_ms,req.recall_started_ms)>=kRecallTimeoutMs) continue;
        if(req.generation != 0 && req.generation != state_.generation) {
            ++state_.fx.rejected;
            continue;
        }
        if(!fx::valid(req.config)) {
            ++state_.fx.errors;
        } else {
            state_.fx.config = req.config;
            state_.fx.request = req.request;
            ++state_.fx.revision;
            state_.fx.dirty = true;
            updated = true;
        }
    }
    if(updated) {
#ifdef SMKP4_SOUND_EDITOR
        applyPerformanceFx();
#else
        state_.fx.applied=state_.fx.config;
        fx::applyBusFx(state_.fx.config);
#endif
    }
}
#ifdef SMKP4_SOUND_EDITOR
void Runtime::finishScene(scene::RecallPhase phase,scene::RecallError error) {
    auto& status=state_.scene;
    if(phase==scene::RecallPhase::Confirmed) {
        status.active=status.pending;
        status.active_domains=status.effective;
        ++status.revision;
    }
    status.phase=phase!=scene::RecallPhase::Confirmed && status.applied_domains
        ? scene::RecallPhase::PartialApply : phase;
    status.error=error;
    status.pending=seq::kNoSlot;
    commands_.scenes.consume();
}

void Runtime::cancelScene() {
    const auto* request=commands_.scenes.front();
    if(!request) return;
    auto& status=state_.scene;
    if(status.token!=request->token) {
        status.token=request->token;
        status.generation=request->generation;
        status.effective=0;
        status.applied_domains=0;
        status.pattern_sequence=0;
        status.patch_error=0;
        status.rejection=scene::Reject::None;
        status.due_phase=status.applied_phase=0;
    }
    if(status.phase==scene::RecallPhase::WaitingPatch && state_.patch_state==PatchState::Requested) {
        state_.patch_state=PatchState::Failed;
        state_.patch_error=-100;
        status.patch_error=state_.patch_error;
        ++state_.patch_failures;
    }
    finishScene(scene::RecallPhase::Cancelled,scene::RecallError::Cancelled);
}

void Runtime::serviceScene(uint32_t now_ms) {
    using namespace scene;
    const auto* request=commands_.scenes.front();
    if(!request) return;
    auto& status=state_.scene;
    const auto position=tempo_.position();
    const bool waiting_patch=status.phase==RecallPhase::WaitingPatch;
    if(waiting_patch) {
        if(commands_.scenes.cancelled(request->token) ||
           usb_.commands.generation()!=usb_generation_) {
            cancelScene();
            return;
        }
        if(eventAgeMs(now_ms,scene_patch_started_ms_)>=kRecallTimeoutMs) {
            if(state_.patch_state==PatchState::Requested) {
                state_.patch_state=PatchState::Failed;
                state_.patch_error=-103;
                status.patch_error=state_.patch_error;
                ++state_.patch_failures;
            }
            finishScene(RecallPhase::Failed,RecallError::PatchTimeout);
            return;
        }
        if(state_.patch_state==PatchState::Requested) return;
        status.patch_error=state_.patch_error;
        if(state_.patch_state!=PatchState::Ready || state_.patch_id!=request->configuration.patch_id ||
            status.generation!=state_.generation || status.generation!=commands_.generation()) {
            finishScene(RecallPhase::Failed,RecallError::PatchFailed);
            return;
        }
    } else if(status.phase!=RecallPhase::WaitingBoundary) {
        status.token=request->token;
        status.generation=request->generation;
        status.pending=request->slot;
        status.due_phase=status.applied_phase=0;
        status.effective=0;
        status.applied_domains=0;
        status.pattern_sequence=0;
        status.patch_error=0;
        status.rejection=Reject::None;
        constexpr auto lock_mask=perf_lock::Fx|perf_lock::Harmony|perf_lock::Arp|perf_lock::Macros;
        if(request->slot>=kSlots || request->mode>RecallMode::NextBar ||
            (request->requested&~kAll) || (request->locks&~lock_mask)) {
            finishScene(RecallPhase::Failed,RecallError::InvalidRequest);
            return;
        }
        status.effective=effectiveDomains(request->configuration.domains,request->requested,
                                          request->locks,request->apply_tempo);
        const auto selection=validateRecall(*request,state_.edit,
            patchIndex(request->configuration.patch_id)>=0,request->pattern.sequence?&request->pattern:nullptr);
        status.rejection=selection.reason;
        if(selection.reason!=Reject::None) {
            finishScene(RecallPhase::Failed,RecallError::InvalidRequest);
            return;
        }
        if(selection.domains&Pattern) status.pattern_sequence=request->pattern.sequence;
        if(request->mode==RecallMode::NextBar) {
            constexpr uint64_t bar=uint64_t(4)*tempo::kPpqn*tempo::kUnitsPerTick;
            status.due_phase=position.phase+bar-position.phase%bar;
            status.phase=RecallPhase::WaitingBoundary;
            status.error=RecallError::None;
        }
    }
    if(commands_.scenes.cancelled(request->token) ||
        (!waiting_patch && request->generation!=commands_.generation())) {
        cancelScene();
        return;
    }
    const auto domains=status.effective;
    const auto& sequencer=sequencer_.status();
    if(!waiting_patch && (state_.patch_state!=PatchState::Ready || request->generation!=state_.generation ||
        ((domains&Fx) && request->fx_revision!=state_.fx.revision) ||
        ((domains&Harmony) && request->harmony_revision!=state_.harmony_revision) ||
        ((domains&Arp) && request->arp_revision!=state_.arp.revision) ||
        ((domains&Macros) && (request->edit_revision!=state_.edit.revision ||
                              request->fx_revision!=state_.fx.revision)) ||
        ((domains&Pattern) && (sequencer.pending_slot!=seq::kNoSlot ||
            sequencer.active_slot!=request->expected_pattern_slot ||
            sequencer_.settings()!=request->expected_pattern)) ||
        ((domains&Tempo) && request->tempo_revision!=position.revision))) {
        finishScene(RecallPhase::Failed,RecallError::Stale);
        return;
    }
    if((domains&Pattern) && !sequencer_.canReplacePattern(request->discard_pattern)) {
        finishScene(RecallPhase::Failed,RecallError::Busy);
        return;
    }
    if(!waiting_patch && request->mode==RecallMode::NextBar &&
        uint64_t(position.phase-status.due_phase)>UINT64_MAX/2) return;

    const auto& config=request->configuration;
    const bool swap=!waiting_patch && (domains&scene::Patch) && config.patch_id!=state_.patch_id;
    if(!waiting_patch && ((domains&Macros) || swap)) {
        const auto& current=state_.edit;
        const auto& positions=(domains&Macros)?config.macros:current.performance_macros;
        const auto& routes=(domains&Macros)?config.routes:current.performance_routes;
        if(swap && !(domains&Macros) && !performance::valid(routes,
            uint8_t(library::find(config.patch_id)->family),edit::factoryPatchCapabilities(config.patch_id))) {
            status.rejection=Reject::IncompatibleMacros;
            finishScene(RecallPhase::Failed,RecallError::InvalidRequest);
            return;
        }
        const auto family=swap?uint8_t(library::find(config.patch_id)->family):current.family;
        // MACROS alone may change synth parameters, but must preserve the
        // effective FX configuration. An explicit FX selection composes both.
        const auto proposed=performance::effectiveFx(state_.fx.config,family,!swap && current.compare,
                                                     positions,routes);
        if(!(domains&Fx) && !fx::fxEquivalent(proposed,performance::effectiveFx(state_.fx.config,current))) {
            finishScene(RecallPhase::Failed,RecallError::FxConflict);
            return;
        }
    }
    if((domains&Tempo) && (request->locks&perf_lock::Fx) && config.bpm!=position.bpm &&
        state_.fx.applied.delay_mode==fx::DelayMode::Sync) {
        finishScene(RecallPhase::Failed,RecallError::UnsupportedDomains);
        return;
    }
    if(!waiting_patch) status.applied_phase=position.phase;
    if(swap) {
        status.phase=RecallPhase::WaitingPatch;
        scene_patch_started_ms_=now_ms;
        status.generation=request->generation+1;
        const auto usb_before=usb_generation_;
        requestPatch(config.patch_id,perf_lock::Fx|perf_lock::Harmony|perf_lock::Arp|perf_lock::Macros);
        if(state_.generation!=status.generation || usb_generation_!=usb_before+1) cancelScene();
        return;
    }
    if(domains&Pattern) {
        bool release=false;
        auto pattern_position=position;
        if(request->mode==RecallMode::NextBar) pattern_position.phase=status.due_phase;
        if(!sequencer_.load(config.pattern_slot,request->pattern.settings,seq::SwitchMode::Immediate,
                            request->discard_pattern,request->token,pattern_position,release)) {
            finishScene(RecallPhase::Failed,RecallError::Busy);
            return;
        }
        if(release) releaseSequencer();
        state_.sequencer_pattern=sequencer_.pattern();
        sequencer_.updateLibraryStatus();
        status.applied_domains|=Pattern;
    }
    if((domains&Macros) || waiting_patch) {
        const auto& current=state_.edit;
        edit::Request edit_request;
        edit_request.patch=current.patch;
        edit_request.generation=state_.generation;
        edit_request.request=(domains&Macros)?request->token:current.request;
        edit_request.mask=current.mask;
        edit_request.compare=current.compare;
        edit_request.slot=current.user_slot;
        std::copy_n(current.macros,3,edit_request.macros);
        std::copy_n(current.manual,edit::Count,edit_request.values);
        edit_request.performance_macros=(domains&Macros)?config.macros:current.performance_macros;
        edit_request.performance_routes=(domains&Macros)?config.routes:current.performance_routes;
        const auto errors=current.errors;
        applyEdit(edit_request);
        if(current.errors!=errors || current.performance_macros!=edit_request.performance_macros ||
           current.performance_routes!=edit_request.performance_routes) {
            finishScene(RecallPhase::Failed,RecallError::Verification);
            return;
        }
        status.applied_domains|=domains&Macros;
    }
    if(domains&Tempo) {
        tempo_.setBpm(config.bpm);
        status.applied_domains|=Tempo;
    }
    if(domains&Harmony) {
        state_.harmony=config.harmony;
        ++state_.harmony_revision;
        status.applied_domains|=Harmony;
    }
    if(domains&Arp) {
        if(!config.arp.enabled && state_.arp.config.enabled) releaseArp();
        arp_.applyConfiguration(config.arp);
        status.applied_domains|=Arp;
    }
    bool fx_verified=true;
    if(domains&Fx) {
        auto expected=performance::effectiveFx(config.fx,state_.edit);
        if(expected.delay_mode==fx::DelayMode::Sync) {
            const float target=tempo::durationMs(expected.delay_division,tempo_.position().bpm);
            expected.delay_time_ms=fx::delayFits(target)?target:
                (last_sync_ms_>0.0f?last_sync_ms_:config.fx.delay_time_ms);
        }
        state_.fx.config=config.fx;
        ++state_.fx.revision;
        state_.fx.dirty=true;
        status.applied_domains|=Fx;
        applyPerformanceFx();
        fx_verified=fx::fxEquivalent(state_.fx.config,config.fx) &&
            fx::fxEquivalent(state_.fx.applied,expected);
    } else if(domains&Tempo) {
        resolveDelay();
    }
    publishArp();
    publishSequencer();
    const bool verified=fx_verified && (!(domains&scene::Patch) || state_.patch_id==config.patch_id) &&
        (!(domains&Pattern) || (state_.sequencer.active_slot==config.pattern_slot &&
            sequencer_.settings()==request->pattern.settings)) &&
        (!(domains&Harmony) || harmony::pack(state_.harmony)==harmony::pack(config.harmony)) &&
        (!(domains&Arp) || state_.arp.config==config.arp) &&
        (!(domains&Tempo) || state_.arp.tempo.bpm==config.bpm);
    if(verified) status.applied_domains|=domains&scene::Patch;
    if(commands_.generation()!=state_.generation ||
       usb_.commands.generation()!=usb_generation_ ||
       commands_.storage_requested.load(std::memory_order_acquire) ||
       commands_.scenes.cancelled(request->token)) {
        cancelScene();
        return;
    }
    finishScene(verified?RecallPhase::Confirmed:RecallPhase::Failed,
                verified?RecallError::None:RecallError::Verification);
}
#endif

void Runtime::apply(uint32_t now_ms) {
#ifdef SMKP4_SOUND_EDITOR
    if(commands_.storage_requested.load(std::memory_order_acquire)) {
        cancelScene();
        if(!state_.storage_quiet){
            silence();amy_execute_deltas();state_.storage_quiet=true;
            commands_.panic(OutputReason::Patch);state_.generation=commands_.generation();
            usb_.commands.panic();usb_generation_=usb_.commands.generation();
        }
        return;
    }
    if(state_.storage_quiet){state_.storage_quiet=false;commands_.panic();state_.generation=commands_.generation();usb_.commands.panic();usb_generation_=usb_.commands.generation();}
#endif
    if(!state_.ready) return;
    const uint32_t generation=commands_.generation();
    if(generation!=state_.generation) {
#ifdef SMKP4_SOUND_EDITOR
        cancelScene();
#endif
#ifdef SMKP4_UNIFIED_ENGINE
        if(state_.patch_state==PatchState::Requested) {
            state_.patch_state=PatchState::Failed;state_.patch_error=-100;++state_.patch_failures;
        }
#endif
        silence(); state_.generation=generation;
        publishOutput(OutputReason::Panic);
        // Global Panic also invalidates queued USB notes/controls.
        usb_.commands.panic(); usb_generation_=usb_.commands.generation();
    }
    const uint32_t depth=commands_.depth()+usb_.commands.depth();
    if(depth>state_.queue_high_water) state_.queue_high_water=depth;
    for(unsigned i=0;i<kVoices;++i) {
        const auto& v=voices_[i];
        if(v.used && groups_[v.group].origin==NoteOrigin::Touch) {
            const bool expired=uint32_t(now_ms-v.started_ms)>=kLeaseMs;
            if(expired || atOrAfter(commands_.released(v.input),v.token)) {
                release(i); if(expired) ++state_.expired_notes;
            }
        }
    }
    Command c;
    for(unsigned i=0;i<kCommandCapacity && commands_.pop(c);++i) {
        if(c.generation!=state_.generation || c.generation!=commands_.generation()) continue;
#ifdef SMKP4_UNIFIED_ENGINE
        if(state_.patch_state!=PatchState::Ready && c.type!=CommandType::Patch && c.type!=CommandType::Master) continue;
#endif
#ifdef SMKP4_SOUND_EDITOR
        if(state_.scene.phase==scene::RecallPhase::WaitingPatch &&
           c.type!=CommandType::Patch && c.type!=CommandType::Master) continue;
#endif
        const uint32_t latency=eventAgeMs(now_ms,c.timestamp_ms);
        if(latency>state_.command_latency_max_ms) state_.command_latency_max_ms=latency;
#ifdef SMKP4_SOUND_EDITOR
        if(c.type==CommandType::Arp) { applyArp(c,false,now_ms);continue; }
#endif
        if(c.type==CommandType::NoteOn) {
            if(c.id>=128 || c.value>127 || !c.token) { ++state_.invalid_commands; continue; }
            const unsigned found=findGroup(NoteOrigin::Touch,0,uint8_t(c.id));
            if(!c.value) {if(found<kVoices && groups_[found].token==c.token)releaseGroup(found);continue;}
            if(atOrAfter(commands_.released(c.id),c.token) ||
               (found<kVoices && groups_[found].token==c.token) || latency>=kLeaseMs)continue;
            if(found<kVoices)releaseGroup(found);
            startGroup(NoteOrigin::Touch,c,now_ms);
        } else if(c.type==CommandType::Harmony) {
#ifdef SMKP4_HARMONY
            const auto config=harmony::unpack(c.token);
            if((c.token>>27) || !harmony::valid(config)){++state_.harmony_errors;continue;}
            state_.harmony=config;++state_.harmony_revision;
#else
            ++state_.invalid_commands;
#endif
        } else if(c.type==CommandType::Patch) {
            if(c.preset_recall && eventAgeMs(now_ms,c.timestamp_ms)>=kRecallTimeoutMs) continue;
            const int index=patchIndex(c.id);
            if(index<0) { ++state_.invalid_commands; continue; }
#ifdef SMKP4_UNIFIED_ENGINE
#ifdef SMKP4_SOUND_EDITOR
            cancelScene();
#endif
            requestPatch(c.id,c.preserve_mask);
            return;
#else
            silence(); usb_.commands.panic(); usb_generation_=usb_.commands.generation();
            bank_=static_cast<uint8_t>(index); state_.patch_id=c.id;
#endif
        } else if(c.type==CommandType::Master && c.value<=127) state_.master=c.value;
        else ++state_.invalid_commands;
    }
#ifdef SMKP4_SOUND_EDITOR
    if(commands_.popArpStop(c)) applyArp(c,false,now_ms);
    if(state_.scene.phase==scene::RecallPhase::WaitingPatch) {
        if(const auto* stop=commands_.sequencer.stop(); stop && stop->generation==state_.generation) {
            applySequencer(*stop,now_ms);
            commands_.sequencer.consumeStop();
        } else {
            serviceScene(now_ms);
        }
        return;
    }
#endif
#ifdef SMKP4_SOUND_EDITOR
    for(unsigned i=0;i<seq::kQueueCapacity;++i) {
        const auto* request=commands_.sequencer.front();
        if(!request || (request->generation!=state_.generation &&
                        request->generation==commands_.generation())) break;
        applySequencer(*request,now_ms);
        commands_.sequencer.consume();
    }
    if(const auto* stop=commands_.sequencer.stop(); stop &&
       (stop->generation==state_.generation || stop->generation!=commands_.generation())) {
        applySequencer(*stop,now_ms);
        commands_.sequencer.consumeStop();
    }
#endif
#ifdef SMKP4_UNIFIED_ENGINE
    if(state_.patch_state==PatchState::Ready) applyUsb(now_ms);
    else if(state_.patch_state!=PatchState::Requested) {
        usb_generation_=usb_.commands.panic();
    }
#else
    applyUsb(now_ms);
#endif
#ifdef SMKP4_SOUND_EDITOR
    bool any_sustain=false;
    for(unsigned i=0;i<16;++i)if(sustain_[i])any_sustain=true;
    state_.sustain_active=any_sustain;
    applyEdit(now_ms);
#endif
    applyFx(now_ms);
    if(commands_.generation()!=state_.generation) {
        silence(); state_.generation=commands_.generation();
        usb_.commands.panic(); usb_generation_=usb_.commands.generation();
    }
#ifdef SMKP4_SOUND_EDITOR
    serviceScene(now_ms);
#endif
}
const int16_t* Runtime::render() {
    if(!state_.ready) return nullptr;
#ifdef SMKP4_UNIFIED_ENGINE
    if(state_.patch_state!=PatchState::Ready) {
        publishOutput(state_.patch_state==PatchState::Failed?OutputReason::Failed:OutputReason::Patch);
        return silent_pcm_;
    }
#ifdef SMKP4_SOUND_EDITOR
    if(state_.storage_quiet){publishOutput(OutputReason::Patch);return silent_pcm_;}
    if(state_.scene.phase==scene::RecallPhase::WaitingPatch) {
        publishOutput(OutputReason::Patch);
        return silent_pcm_;
    }
#endif
    if(amy_get_oom_count()!=state_.oom_count) { failRendering();return silent_pcm_; }
#endif
#ifdef SMKP4_SOUND_EDITOR
    serviceDelay();
    if(arp_.status().config.enabled) serviceArp();
    if(sequencer_.status().playing && commands_.generation()==state_.generation) serviceSequencer();
#endif
    publishOutput(OutputReason::Ready);
    for(unsigned i=0;i<kVoices;++i)
        if(voices_[i].used && voices_[i].usb && modulation_[voices_[i].channel]) tune(i);
    vibrato_frame_=(vibrato_frame_+kBlockFrames)%kSampleRateHz;
    amy_execute_deltas(); amy_render(0,AMY_OSCS,0);
    int16_t* pcm=amy_fill_buffer();
#ifdef SMKP4_UNIFIED_ENGINE
    if(amy_get_oom_count()!=state_.oom_count) { failRendering();return silent_pcm_; }
#endif
    const int32_t target=static_cast<int32_t>(state_.master)*256; // <= 0.5 FS master
    uint32_t peak=0;
    for(unsigned f=0;f<kBlockFrames;++f) {
        if(gain_q16_<target) gain_q16_+=target-gain_q16_>64?64:target-gain_q16_;
        else if(gain_q16_>target) gain_q16_-=gain_q16_-target>64?64:gain_q16_-target;
        for(unsigned ch=0;ch<2;++ch) {
            const unsigned i=f*2+ch;
            const int32_t raw=pcm[i];
            if(raw>=32767 || raw<=-32768) ++state_.clipped;
            pcm[i]=static_cast<int16_t>(raw*gain_q16_/65536);
            const uint32_t magnitude=pcm[i]<0?-static_cast<int32_t>(pcm[i]):pcm[i];
            if(magnitude>peak) peak=magnitude;
        }
    }
#ifdef SMKP4_SOUND_EDITOR
    tempo_.advance(kBlockFrames);
    state_.arp.tempo=tempo_.position();
    state_.sequencer.bpm=tempo_.position().bpm;
#endif
    state_.peak=peak; ++state_.blocks; state_.oom_count=amy_get_oom_count();
    return pcm;
}

void Runtime::failRendering() {
    // No allocator or new delta events on an exhausted resource path.
    amy_deltas_reset();all_notes_off();
#ifdef SMKP4_SOUND_EDITOR
    arp_.stop();arp_identity_=0;publishArp();
    stopSequencer();
#endif
    for(auto& voice:voices_)voice=Voice{};
    for(auto& g:groups_)g=Group{};
    std::memset(sustain_,0,sizeof(sustain_));
    std::memset(bend_,0,sizeof(bend_));std::memset(modulation_,0,sizeof(modulation_));
    state_.active_notes=0;state_.patch_state=PatchState::Failed;state_.patch_error=-102;
    state_.oom_count=amy_get_oom_count();++state_.patch_failures;
    commands_.panic();state_.generation=commands_.generation();
    usb_.commands.panic();usb_generation_=usb_.commands.generation();gain_q16_=0;
}

bool Runtime::verifyReusable(uint16_t patch_id) {
    const auto* patch=library::find(patch_id);
    if(!patch || amy_get_oom_count()!=load_oom_baseline_) return false;
    bool seen[kOscillators]{};
    for(unsigned id=1;id<=kVoices;++id) {
        uint16_t voice;
        if(instrument_get_patch_number(id)!=patch_id ||
           instrument_get_num_voices(id,&voice)!=1 || voice>=kAmyVoices ||
           instrument_get_oscs_per_voice(id)!=patch->oscillators) return false;
        const unsigned base=voice_to_base_osc[voice];
        if(base+patch->oscillators>kOscillators) return false;
        if(patch->family==library::SynthFamily::FM) {
            if(!synth[base] || synth[base]->wave!=ALGO) return false;
            for(unsigned op=0;op<MAX_ALGO_OPS;++op) {
                const unsigned source=synth[base]->algo_source[op];
                if(source>=kOscillators || osc_to_voice[source]!=voice || !synth[source])return false;
            }
        }
        unsigned owned=0;
        for(unsigned o=0;o<kOscillators;++o) if(osc_to_voice[o]==voice) {
            if(seen[o] || o<base || o>=base+patch->oscillators || !synth[o]) return false;
            seen[o]=true; ++owned;
        }
        if(owned!=patch->oscillators) return false;
    }
    return true;
}
bool Runtime::loadReusable(uint16_t patch_id) {
    // Drop the complete old layout before assigning differently sized voices.
    // This is quiescent reconstruction, NOT transactional rollback.
    amy_deltas_reset(); all_notes_off();
    for(unsigned id=1;id<=kVoices;++id) {
        auto e=amy_default_event();e.synth=id;e.num_voices=0;e.time=0;
        const auto result=patches_load_patch_checked(&e);
        if(result!=AMY_PATCH_RESOURCES_ASSIGNED) { state_.patch_error=result;return false; }
    }
    amy_execute_deltas();
    auto neutral=amy_default_event();neutral.bus=0;
    neutral.eq_l=neutral.eq_m=neutral.eq_h=0;amy_add_event(&neutral);
    for(unsigned id=1;id<=kVoices;++id) {
        if(commands_.generation()!=state_.generation) { state_.patch_error=-100;return false; }
        auto e=amy_default_event();e.synth=id;e.patch_number=patch_id;
        e.num_voices=1;e.bus=0;e.synth_delay_ms=0;e.time=0;
        const auto result=patches_load_patch_checked(&e);
        if(result!=AMY_PATCH_RESOURCES_ASSIGNED) { state_.patch_error=result;return false; }
    }
    amy_execute_deltas();
    state_.fx.applied=state_.fx.config;
#ifdef SMKP4_SOUND_EDITOR
    resolveDelay();
    fx::applyBusFx(state_.fx.applied,false);
    serviceDelay(true);
#else
    fx::applyBusFx(state_.fx.config);
#endif
    state_.patch_state=PatchState::Verifying;
    // Warm lazy DSP on the worker while output continues zero-filled PCM.
    amy_simple_fill_buffer();
    return verifyReusable(patch_id);
}
void Runtime::requestPatch(uint16_t patch_id,uint8_t preserve_mask) {
    silence(); amy_execute_deltas(); gain_q16_=0;
    // Discard all events admitted before this request was applied.
    state_.generation=commands_.panic(OutputReason::Patch);
    usb_generation_=usb_.commands.panic();
    state_.requested_patch_id=patch_id;
    state_.patch_error=0;
    requested_preserve_mask_=preserve_mask;
    state_.patch_state=PatchState::Requested;
    publishOutput(OutputReason::Patch);
}
void Runtime::loadPendingPatch() {
#ifdef SMKP4_UNIFIED_ENGINE
    if(!started_ || state_.patch_state!=PatchState::Requested) return;
#ifdef SMKP4_SOUND_EDITOR
    const bool scene_patch=state_.scene.phase==scene::RecallPhase::WaitingPatch;
    if(scene_patch && (state_.scene.generation!=commands_.generation() ||
        usb_.commands.generation()!=usb_generation_ ||
        commands_.storage_requested.load(std::memory_order_acquire) ||
        commands_.scenes.cancelled(state_.scene.token))) {
        cancelScene();
        return;
    }
#endif
    load_oom_baseline_=amy_get_oom_count();
    state_.patch_state=PatchState::Loading;
    publishOutput(OutputReason::Patch);
    const uint8_t mask=requested_preserve_mask_;
    const fx::FxConfig prev_fx=state_.fx.config;
    const harmony::Config prev_harmony=state_.harmony;
#ifdef SMKP4_SOUND_EDITOR
    const performance::Positions prev_macros=state_.edit.performance_macros;
    const performance::RouteConfiguration prev_routes=state_.edit.performance_routes;
#endif
    const bool loaded=loadReusable(state_.requested_patch_id);
    const bool cancelled=commands_.generation()!=state_.generation ||
        usb_.commands.generation()!=usb_generation_;
    // Invalidate all traffic admitted DURING loading, not only before it.
    const auto previous_generation=state_.generation;
    state_.generation=commands_.panic(OutputReason::Patch);
    const auto previous_usb_generation=usb_generation_;
    usb_generation_=usb_.commands.panic();
    silence();amy_execute_deltas();gain_q16_=0;
    if(!loaded || cancelled || state_.generation!=previous_generation+1 ||
        usb_generation_!=previous_usb_generation+1 || amy_get_oom_count()!=load_oom_baseline_) {
        if(!state_.patch_error) state_.patch_error=cancelled?-100:-101;
        state_.patch_state=PatchState::Failed;++state_.patch_failures;
        // Explicit retry of a valid patch may reconstruct the full set. Until
        // then no notes enter a partially loaded engine. No fictitious rollback.
    } else {
        state_.patch_id=state_.requested_patch_id;bank_=0;
        state_.patch_state=PatchState::Ready;++state_.patch_changes;
        state_.preserve_mask=mask;
        if(mask & perf_lock::Fx) {
            state_.fx.config=prev_fx;
            state_.fx.dirty=!fx::fxEquivalent(prev_fx, fx::FxConfig{});
        } else {
            state_.fx.config=fx::FxConfig{};
            state_.fx.dirty=false;
            for (auto& source : state_.fx.performance_sources) source = 0;
        }
        if(mask & perf_lock::Harmony) {
            state_.harmony=prev_harmony;
        } else {
            state_.harmony=harmony::Config{};
        }
#ifdef SMKP4_SOUND_EDITOR
        if(!scene_patch) last_sync_ms_=0.0f;
        captureEdit();
        if(mask & perf_lock::Macros) {
            if(performance::valid(prev_routes, state_.edit.family, state_.edit.available)) {
                state_.edit.performance_routes=prev_routes;
                state_.edit.performance_macros=prev_macros;
                state_.edit.dirty=prev_routes.custom || prev_macros!=performance::kNeutral;
            }
        }
        if(mask & perf_lock::Arp) {
            releaseArp();
            arp_.stop();
        } else {
            releaseArp();
            arp_.stop();
            arp_.configure(arp::Action::Enable, 0);
            arp_.configure(arp::Action::Mode, uint8_t(arp::Mode::Up));
            arp_.configure(arp::Action::Rate, uint8_t(arp::Rate::Sixteenth));
            arp_.configure(arp::Action::Octaves, 2);
            arp_.configure(arp::Action::Gate, 65);
            arp_.configure(arp::Action::Swing, 54);
            arp_.configure(arp::Action::Latch, 0);
            arp_.configure(arp::Action::Restart, 0);
        }
        publishArp();
        applyPerformanceFx();
        serviceDelay(true);
#else
        state_.fx.applied=state_.fx.config;
        fx::applyBusFx(state_.fx.config);
#endif
    }
    state_.oom_count=amy_get_oom_count();
#ifdef SMKP4_SOUND_EDITOR
    if(scene_patch) {
        ++state_.scene.generation;
        if(state_.patch_state==PatchState::Ready) {
            state_.scene.applied_domains|=scene::Patch;
            if(state_.edit.available!=edit::factoryPatchCapabilities(state_.patch_id))
                finishScene(scene::RecallPhase::Failed,scene::RecallError::Verification);
        }
        else {
            state_.scene.patch_error=state_.patch_error;
            finishScene(scene::RecallPhase::Failed,scene::RecallError::PatchFailed);
        }
    }
#endif
    publishOutput(state_.patch_state==PatchState::Ready?OutputReason::Ready:OutputReason::Failed);
#endif
}
}

#ifdef SMKP4_SOUND_EDITOR
#include "sound_edit_runtime.inc"
#endif
