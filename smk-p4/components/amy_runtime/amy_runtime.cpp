#include "amy_runtime.h"
#include <cstring>
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
bool Runtime::begin(uint32_t sample_rate_hz) {
    if(started_ || sample_rate_hz!=kSampleRateHz) return false;
    state_=Snapshot{}; bank_=0; gain_q16_=0;
    std::memset(tokens_,0,sizeof(tokens_));
    commands_.panic(); state_.generation=commands_.generation();
    auto config=amy_default_config();
    config.features.default_synths=config.features.startup_bleep=0;
    config.features.reverb=config.features.chorus=config.features.echo=0;
    config.features.partials=config.features.custom=0;
    config.platform.multicore=config.platform.multithread=0;
    config.ks_oscs=0; config.max_oscs=kOscillators;
    config.max_voices=kVoices*kPatchCount; config.max_synths=kPatchCount+1;
    config.max_memory_patches=1; config.max_sequencer_tags=1;
#ifdef ESP_PLATFORM
    config.ram_caps_synth=config.ram_caps_events=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
    config.ram_caps_block=config.ram_caps_fbl=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
    config.ram_caps_delay=config.ram_caps_sample=config.ram_caps_sysex=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
#endif
    amy_start(config); started_=true;
    for(size_t i=0;i<kPatchCount;++i) {
        auto e=amy_default_event(); e.synth=static_cast<uint8_t>(i+1);
        e.patch_number=kPatches[i].id; e.num_voices=kVoices; e.bus=0; e.synth_delay_ms=0;
        amy_add_event(&e);
        uint16_t voices[kVoices];
        if(instrument_get_num_voices(e.synth,voices)!=kVoices) { end(); return false; }
        const int oscs=instrument_get_oscs_per_voice(e.synth);
        for(auto voice:voices) {
            unsigned count=0;
            for(unsigned o=0;o<kOscillators;++o) if(osc_to_voice[o]==voice) ++count;
            if(oscs!=8 || count!=static_cast<unsigned>(oscs)) { end(); return false; }
        }
    }
    // Materialize all patch deltas and warm DSP before the timed audio service.
    for(unsigned i=0;i<2;++i) amy_simple_fill_buffer();
    silence(); amy_execute_deltas();
    state_.oom_count=amy_get_oom_count();
    if(state_.oom_count) { end(); return false; }
    state_.ready=true; return true;
}
void Runtime::end() {
    if(started_) { silence(); amy_stop(); started_=false; }
    state_.ready=false; state_.active_notes=0;
    std::memset(tokens_,0,sizeof(tokens_));
    commands_.panic();
}
void Runtime::silence() {
    amy_deltas_reset();
    for(size_t i=0;i<kPatchCount;++i) {
        auto e=amy_default_event(); e.synth=static_cast<uint8_t>(i+1); e.pedal=0; e.velocity=0;
        amy_add_event(&e);
    }
    all_notes_off();
    std::memset(tokens_,0,sizeof(tokens_)); state_.active_notes=0;
}
void Runtime::note(uint8_t pitch,uint8_t velocity) {
    auto e=amy_default_event(); e.synth=bank_+1; e.midi_note=pitch; e.velocity=velocity/127.0f;
    amy_add_event(&e);
}
void Runtime::apply(uint32_t now_ms) {
    if(!state_.ready) return;
    const uint32_t generation=commands_.generation();
    if(generation!=state_.generation) { silence(); state_.generation=generation; }
    const uint32_t depth=commands_.depth();
    if(depth>state_.queue_high_water) state_.queue_high_water=depth;
    for(unsigned n=0;n<128;++n) if(tokens_[n]) {
        const bool expired=uint32_t(now_ms-started_ms_[n])>=kLeaseMs;
        if(expired || atOrAfter(commands_.released(n),tokens_[n])) {
            note(static_cast<uint8_t>(n),0); tokens_[n]=0; --state_.active_notes;
            if(expired) ++state_.expired_notes;
        }
    }
    Command c;
    for(unsigned i=0;i<kCommandCapacity && commands_.pop(c);++i) {
        if(c.generation!=state_.generation || c.generation!=commands_.generation()) continue;
        const uint32_t latency=now_ms-c.timestamp_ms;
        if(latency>state_.command_latency_max_ms) state_.command_latency_max_ms=latency;
        if(c.type==CommandType::NoteOn) {
            if(c.id>=128 || c.value>127 || !c.token) { ++state_.invalid_commands; continue; }
            if(!c.value) { if(tokens_[c.id]==c.token) { note(c.id,0); tokens_[c.id]=0; --state_.active_notes; } continue; }
            if(atOrAfter(commands_.released(c.id),c.token) || tokens_[c.id]==c.token || latency>=kLeaseMs) continue;
            if(!tokens_[c.id] && state_.active_notes>=kVoices) { ++state_.invalid_commands; continue; }
            if(tokens_[c.id]) note(c.id,0); else ++state_.active_notes;
            tokens_[c.id]=c.token; started_ms_[c.id]=now_ms; note(c.id,c.value);
        } else if(c.type==CommandType::Patch) {
            const int index=patchIndex(c.id);
            if(index<0) { ++state_.invalid_commands; continue; }
            silence(); bank_=static_cast<uint8_t>(index); state_.patch_id=c.id;
        } else if(c.type==CommandType::Master && c.value<=127) state_.master=c.value;
        else ++state_.invalid_commands;
    }
    // Panic arriving during the bounded drain must also cancel this block.
    if(commands_.generation()!=state_.generation) { silence(); state_.generation=commands_.generation(); }
}
const int16_t* Runtime::render() {
    if(!state_.ready) return nullptr;
    amy_execute_deltas(); amy_render(0,AMY_OSCS,0);
    int16_t* pcm=amy_fill_buffer();
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
}
