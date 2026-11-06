#include "amy_runtime.h"
#include <cstring>
#include <cmath>
extern "C" {
#include "amy.h"
void amy_platform_init() {}
void amy_platform_deinit() {}
void delay_ms(uint32_t) {}
void amy_update_tasks() {}
int16_t* amy_render_audio() { return amy_simple_fill_buffer(); }
size_t amy_i2s_write(const uint8_t*,size_t bytes) { return bytes; }
}
namespace smk::p4::amy {
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
    for(auto& v:voices_) v=Voice{};
    commands_.panic(); state_.generation=commands_.generation();
    auto config=amy_default_config();
    config.features.default_synths=config.features.startup_bleep=0;
    config.features.reverb=config.features.chorus=config.features.echo=0;
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
    state_.ready=true; return true;
}
void Runtime::end() {
    if(started_) { silence(); amy_stop(); started_=false; }
    state_.ready=false; state_.active_notes=0;
    for(auto& v:voices_) v=Voice{};
    commands_.panic();
}
void Runtime::silence() {
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
    std::memset(bend_,0,sizeof(bend_)); std::memset(modulation_,0,sizeof(modulation_));
    for(auto& v:voices_) v=Voice{};
    state_.active_notes=0;
}
void Runtime::note(unsigned slot,uint8_t velocity) {
    const auto& v=voices_[slot];
    auto e=amy_default_event(); e.synth=bank_*kVoices+slot+1;
    e.midi_note=v.pitch; e.velocity=velocity/127.0f; amy_add_event(&e);
    if(velocity && v.usb) tune(slot);
}
void Runtime::tune(unsigned slot) {
    const auto& v=voices_[slot];
    auto e=amy_default_event(); e.synth=bank_*kVoices+slot+1;
    // AMY pitch_bend is global. Per-voice fractional note updates preserve
    // channel and Touch isolation, without retriggering velocity/envelopes.
    constexpr float tau=6.28318530718f;
    const float vibrato=0.5f*(modulation_[v.channel]/127.0f)*
        std::sin(tau*5.0f*vibrato_frame_/kSampleRateHz);
    e.midi_note=v.pitch+bend_[v.channel]*(2.0f/8192.0f)+vibrato;
    amy_add_event(&e);
}
void Runtime::release(unsigned slot) {
    if(!voices_[slot].used) return;
    note(slot,0); voices_[slot]=Voice{}; --state_.active_notes;
}
void Runtime::cancelUsb() {
    for(unsigned i=0;i<kVoices;++i) if(voices_[i].used && voices_[i].usb) release(i);
    std::memset(sustain_,0,sizeof(sustain_));
    std::memset(bend_,0,sizeof(bend_)); std::memset(modulation_,0,sizeof(modulation_));
}
void Runtime::applyUsb(uint32_t now_ms) {
    auto& q=usb_.commands;
    if(q.generation()!=usb_generation_) { cancelUsb(); usb_generation_=q.generation(); }
    Command c;
    unsigned applied=0;
    for(unsigned scanned=0;scanned<kUsbCommandCapacity && applied<kCommandCapacity && q.pop(c);++scanned) {
        if(c.generation!=usb_generation_ || c.generation!=q.generation()) continue;
        ++applied;
        const uint32_t latency=eventAgeMs(now_ms,c.timestamp_ms);
        if(latency>state_.command_latency_max_ms) state_.command_latency_max_ms=latency;
        if(c.channel>=16) { ++state_.invalid_commands; continue; }
        if(c.type==CommandType::NoteOn || c.type==CommandType::NoteOff) {
            if(c.id>=128 || c.value>127) { ++state_.invalid_commands; continue; }
            unsigned found=kVoices,free=kVoices;
            for(unsigned i=0;i<kVoices;++i) {
                const auto& v=voices_[i];
                if(!v.used && free==kVoices) free=i;
                if(v.used && v.usb && v.channel==c.channel && v.pitch==c.id) found=i;
            }
            if(c.type==CommandType::NoteOff || !c.value) {
                if(found<kVoices) { voices_[found].held=false; if(!sustain_[c.channel]) release(found); }
            } else {
                if(latency>=kLeaseMs) continue; // Never replay an old queued gate.
                if(found<kVoices && voices_[found].held) continue; // Idempotent while held.
                if(found<kVoices) { release(found); free=found; }
                if(free==kVoices) { ++state_.polyphony_rejected; continue; }
                voices_[free]={true,true,true,static_cast<uint8_t>(c.id),c.channel,0,now_ms};
                ++state_.active_notes; note(free,c.value);
            }
        } else if(c.type==CommandType::Control && c.value<=127) {
            if(c.id==64) {
                sustain_[c.channel]=c.value>=64;
                if(!sustain_[c.channel]) for(unsigned i=0;i<kVoices;++i) {
                    const auto& v=voices_[i];
                    if(v.used && v.usb && v.channel==c.channel && !v.held) release(i);
                }
            } else if(c.id==120 || c.id==123) {
                // Safety policy: both release this USB channel immediately.
                sustain_[c.channel]=false;
                for(unsigned i=0;i<kVoices;++i) if(voices_[i].used && voices_[i].usb && voices_[i].channel==c.channel) release(i);
            } else if(c.id==1) {
                modulation_[c.channel]=c.value;
                for(unsigned i=0;i<kVoices;++i) if(voices_[i].used && voices_[i].usb && voices_[i].channel==c.channel) tune(i);
            }
        } else if(c.type==CommandType::Bend && c.id<16384) {
            bend_[c.channel]=static_cast<int16_t>(int(c.id)-8192);
            for(unsigned i=0;i<kVoices;++i) if(voices_[i].used && voices_[i].usb && voices_[i].channel==c.channel) tune(i);
        } else ++state_.invalid_commands;
    }
    if(q.generation()!=usb_generation_) { cancelUsb(); usb_generation_=q.generation(); }
}
void Runtime::apply(uint32_t now_ms) {
    if(!state_.ready) return;
    const uint32_t generation=commands_.generation();
    if(generation!=state_.generation) {
#ifdef SMKP4_UNIFIED_ENGINE
        if(state_.patch_state==PatchState::Requested) {
            state_.patch_state=PatchState::Failed;state_.patch_error=-100;++state_.patch_failures;
        }
#endif
        silence(); state_.generation=generation;
        // Global Panic also invalidates queued USB notes/controls.
        usb_.commands.panic(); usb_generation_=usb_.commands.generation();
    }
    const uint32_t depth=commands_.depth()+usb_.commands.depth();
    if(depth>state_.queue_high_water) state_.queue_high_water=depth;
    for(unsigned i=0;i<kVoices;++i) {
        const auto& v=voices_[i];
        if(v.used && !v.usb) {
            const bool expired=uint32_t(now_ms-v.started_ms)>=kLeaseMs;
            if(expired || atOrAfter(commands_.released(v.pitch),v.token)) {
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
        const uint32_t latency=eventAgeMs(now_ms,c.timestamp_ms);
        if(latency>state_.command_latency_max_ms) state_.command_latency_max_ms=latency;
        if(c.type==CommandType::NoteOn) {
            if(c.id>=128 || c.value>127 || !c.token) { ++state_.invalid_commands; continue; }
            unsigned found=kVoices,free=kVoices;
            for(unsigned j=0;j<kVoices;++j) {
                const auto& v=voices_[j];
                if(!v.used && free==kVoices) free=j;
                if(v.used && !v.usb && v.pitch==c.id) found=j;
            }
            if(!c.value) { if(found<kVoices && voices_[found].token==c.token) release(found); continue; }
            if(atOrAfter(commands_.released(c.id),c.token) ||
               (found<kVoices && voices_[found].token==c.token) || latency>=kLeaseMs) continue;
            if(found<kVoices) { release(found); free=found; }
            if(free==kVoices) { ++state_.invalid_commands; ++state_.polyphony_rejected; continue; }
            voices_[free]={true,false,true,static_cast<uint8_t>(c.id),0,c.token,now_ms};
            ++state_.active_notes; note(free,c.value);
        } else if(c.type==CommandType::Patch) {
            const int index=patchIndex(c.id);
            if(index<0) { ++state_.invalid_commands; continue; }
#ifdef SMKP4_UNIFIED_ENGINE
            silence(); amy_execute_deltas(); gain_q16_=0;
            // Barrier discards all events admitted before this request was applied.
            commands_.panic(); state_.generation=commands_.generation();
            usb_.commands.panic(); usb_generation_=usb_.commands.generation();
            state_.requested_patch_id=c.id; state_.patch_error=0;
            state_.patch_state=PatchState::Requested;
            return;
#else
            silence(); usb_.commands.panic(); usb_generation_=usb_.commands.generation();
            bank_=static_cast<uint8_t>(index); state_.patch_id=c.id;
#endif
        } else if(c.type==CommandType::Master && c.value<=127) state_.master=c.value;
        else ++state_.invalid_commands;
    }
#ifdef SMKP4_UNIFIED_ENGINE
    if(state_.patch_state==PatchState::Ready) applyUsb(now_ms);
    else { usb_.commands.panic();usb_generation_=usb_.commands.generation(); }
#else
    applyUsb(now_ms);
#endif
    if(commands_.generation()!=state_.generation) {
        silence(); state_.generation=commands_.generation();
        usb_.commands.panic(); usb_generation_=usb_.commands.generation();
    }
}
const int16_t* Runtime::render() {
    if(!state_.ready) return nullptr;
#ifdef SMKP4_UNIFIED_ENGINE
    if(state_.patch_state!=PatchState::Ready) return silent_pcm_;
    if(amy_get_oom_count()!=state_.oom_count) { failRendering();return silent_pcm_; }
#endif
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
    state_.peak=peak; ++state_.blocks; state_.oom_count=amy_get_oom_count();
    return pcm;
}

void Runtime::failRendering() {
    // No allocator or new delta events on an exhausted resource path.
    amy_deltas_reset();all_notes_off();
    for(auto& voice:voices_)voice=Voice{};
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
    state_.patch_state=PatchState::Verifying;
    // Warm lazy DSP on the worker while output continues zero-filled PCM.
    amy_simple_fill_buffer();
    return verifyReusable(patch_id);
}
void Runtime::loadPendingPatch() {
#ifdef SMKP4_UNIFIED_ENGINE
    if(!started_ || state_.patch_state!=PatchState::Requested) return;
    load_oom_baseline_=amy_get_oom_count();
    state_.patch_state=PatchState::Loading;
    const bool loaded=loadReusable(state_.requested_patch_id);
    const bool cancelled=commands_.generation()!=state_.generation ||
        usb_.commands.generation()!=usb_generation_;
    // Invalidate all traffic admitted DURING loading, not only before it.
    commands_.panic();state_.generation=commands_.generation();
    usb_.commands.panic();usb_generation_=usb_.commands.generation();
    silence();amy_execute_deltas();gain_q16_=0;
    if(!loaded || cancelled || amy_get_oom_count()!=load_oom_baseline_) {
        if(!state_.patch_error) state_.patch_error=cancelled?-100:-101;
        state_.patch_state=PatchState::Failed;++state_.patch_failures;
        // Explicit retry of a valid patch may reconstruct the full set. Until
        // then no notes enter a partially loaded engine. No fictitious rollback.
    } else {
        state_.patch_id=state_.requested_patch_id;bank_=0;
        state_.patch_state=PatchState::Ready;++state_.patch_changes;
    }
    state_.oom_count=amy_get_oom_count();
#endif
}
}
