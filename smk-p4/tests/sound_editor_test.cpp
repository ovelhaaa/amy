#include "amy_runtime.h"
#include "local_performance_backend.h"
#include "user_presets.h"
#include <cassert>
#include <cstdio>
#include <cstring>
extern "C" {
#include "amy.h"
void probe_realtime(bool);size_t probe_bytes();uint32_t probe_calls();
}
using namespace smk::p4;
struct MemoryStore:edit::PresetStore {
    uint8_t bytes[16][2][128]{};bool present[16][2]{};bool fail=false,torn=false;
    bool read(unsigned s,unsigned b,uint8_t* out,unsigned& n) override {if(!present[s][b] || n<128)return false;std::memcpy(out,bytes[s][b],128);n=128;return true;}
    bool write(unsigned s,unsigned b,const uint8_t* data,unsigned n) override {if(fail)return false;if(torn){std::memset(bytes[s][b],0,128);std::memcpy(bytes[s][b],data,63);present[s][b]=true;return false;}assert(n==128);std::memcpy(bytes[s][b],data,n);present[s][b]=true;return true;}
};
static amy::Runtime runtime;
static void patch(unsigned id){runtime.commands().panic();assert(runtime.commands().push({amy::CommandType::Patch,uint16_t(id)}));runtime.apply(1);runtime.loadPendingPatch();assert(runtime.snapshot().patch_state==amy::PatchState::Ready);}
static void tick(){probe_realtime(true);runtime.apply(20);assert(runtime.render());probe_realtime(false);}
static edit::Request request(){const auto& s=runtime.snapshot();edit::Request r;r.patch=s.patch_id;r.generation=s.generation;r.request=s.edit.request+1;for(unsigned i=0;i<edit::Count;++i)r.values[i]=s.edit.manual[i];r.mask=s.edit.mask;return r;}
static void submit(edit::Request r){assert(runtime.commands().edits.push(r));tick();assert(runtime.snapshot().edit.request==r.request);}
int main(){std::setvbuf(stdout,nullptr,_IONBF,0);
    assert(runtime.begin());patch(4);
    auto state=runtime.snapshot().edit;
    std::printf("Juno available=%x cutoff=%g Q=%g attack=%g release=%g Runtime=%zu State=%zu\n",state.available,state.original[0],state.original[1],state.original[2],state.original[3],sizeof(runtime),sizeof(state));
    for(unsigned o=0;o<6;++o)std::printf("osc%u wave%u filter%u bp%u amp_eg%g\n",o,synth[o]->wave,synth[o]->filter_type,synth[o]->max_num_breakpoints[0],synth[o]->amp_coefs[COEF_EG0]);
    assert((state.available&15)==15);
    assert((state.available & ((1u<<edit::Decay)|(1u<<edit::Sustain))) == ((1u<<edit::Decay)|(1u<<edit::Sustain)));
    for(unsigned n=60;n<68;++n){const uint8_t packet[]={0x09,0x90,uint8_t(n),100};assert(runtime.usbInput().packet(packet,0));}
    tick();assert(runtime.snapshot().active_notes==8);
    uint32_t clocks[64]{};for(unsigned o=0;o<64;++o)if(synth[o])clocks[o]=synth[o]->note_on_clock;
    const auto allocations=probe_calls();const auto memory=probe_bytes();
    auto r=request();r.mask=1;r.values[0]=500;submit(r);
    for(unsigned o=0;o<48;++o){assert(synth[o]->note_on_clock==clocks[o]);if(synth[o]->filter_type!=FILTER_NONE)assert(std::fabs(freq_of_logfreq(synth[o]->filter_logfreq_coefs[0])-500)<0.1f);}
    assert(runtime.snapshot().active_notes==8);
    r=request();r.mask|=14;r.values[1]=1.5;r.values[2]=750;r.values[3]=1200;submit(r);
    for(unsigned v=0;v<8;++v){uint16_t voice;assert(instrument_get_num_voices(v+1,&voice)==1);const unsigned o=voice_to_base_osc[voice];assert(synth[o]->breakpoint_times[0][0]==36000);assert(synth[o]->breakpoint_times[0][2]==57600);}
    const auto other_decay=synth[1]->breakpoint_times[0][1];const auto other_sustain=synth[1]->breakpoint_values[0][1];
    r=request();r.mask|=(1u<<edit::Decay)|(1u<<edit::Sustain);r.values[edit::Decay]=1500;r.values[edit::Sustain]=10;submit(r);
    for(unsigned v=0;v<8;++v){uint16_t voice;assert(instrument_get_num_voices(v+1,&voice)==1);const unsigned o=voice_to_base_osc[voice];assert(synth[o]->breakpoint_times[0][1]==72000);assert(std::fabs(synth[o]->breakpoint_values[0][1]-0.1f)<0.0001f);assert(synth[o+1]->breakpoint_times[0][1]==other_decay);assert(synth[o+1]->breakpoint_values[0][1]==other_sustain);}
    r=request();r.compare=true;submit(r);assert(runtime.snapshot().edit.compare && (runtime.snapshot().edit.mask&15)==15);
    r=request();r.compare=false;submit(r);assert(runtime.snapshot().edit.applied[0]==500);
    r=request();r.mask=0;submit(r);assert(!runtime.snapshot().edit.dirty);
    for(unsigned n=0;n<1000;++n){r=request();r.mask=1;r.values[0]=float(200+n%100);submit(r);}
    assert(probe_calls()==allocations && probe_bytes()==memory);
    const auto errors=runtime.snapshot().edit.errors;r=request();r.values[0]=NAN;submit(r);assert(runtime.snapshot().edit.errors==errors+1);
    r=request();r.generation--;submit(r);assert(runtime.snapshot().edit.errors==errors+2);
    runtime.commands().panic();tick();assert(runtime.snapshot().active_notes==0);
    harmony::Config harmony;harmony.chord_on=true;harmony.chord=1;
    assert(runtime.commands().push({amy::CommandType::Harmony,0,0,0,harmony::pack(harmony)}));tick();
    const uint8_t chord_on[]={9,0x90,60,100};assert(runtime.usbInput().packet(chord_on,0));tick();
    const auto group=runtime.snapshot().last_group;assert(runtime.snapshot().active_notes==3);
    r=request();r.mask=1;r.values[0]=700;submit(r);assert(runtime.snapshot().last_group==group && runtime.snapshot().active_notes==3);
    const uint8_t chord_off[]={8,0x80,60,0};assert(runtime.usbInput().packet(chord_off,0));tick();assert(runtime.snapshot().active_notes==0);
    patch(135);assert(runtime.snapshot().edit.family==0);
    state=runtime.snapshot().edit;std::printf("FM available=%x\n",state.available);
    assert((state.available&0x3f0)==0x3f0);
    uint16_t source[8][6];float amps[8][6];
    for(unsigned v=0;v<8;++v){uint16_t voice;assert(instrument_get_num_voices(v+1,&voice)==1);auto* a=synth[voice_to_base_osc[voice]];
        for(unsigned op=0;op<6;++op){source[v][op]=a->algo_source[5-op];amps[v][op]=synth[source[v][op]]->amp_coefs[0];}}
    for(unsigned op=0;op<6;++op){r=request();r.mask=1u<<(edit::OpLevel0+op);r.values[edit::OpLevel0+op]=50;submit(r);
        for(unsigned v=0;v<8;++v)assert(synth[source[v][op]]->amp_coefs[0]==amps[v][op]*0.5f);}
    for(unsigned op=0;op<6;++op)if(state.available&(1u<<(edit::OpRatio0+op))){r=request();r.mask=1u<<(edit::OpRatio0+op);r.values[edit::OpRatio0+op]=2;submit(r);for(unsigned v=0;v<8;++v)assert(synth[source[v][op]]->logratio==1);}
    r=request();r.mask=1u<<edit::Depth;r.values[edit::Depth]=0.75f;submit(r);
    for(unsigned v=0;v<8;++v)assert(synth[source[v][0]]->amp_coefs[0]==amps[v][0]); // OP1 carrier preserved
    assert(synth[source[0][1]]->amp_coefs[0]==amps[0][1]*0.75f);
    MemoryStore store;edit::Preset preset;preset.sequence=1;preset.patch=135;preset.family=0;preset.mask=1u<<edit::Depth;preset.values[edit::Depth]=0.75f;std::strcpy(preset.name,"MY DX PIANO");
    for(unsigned slot=0;slot<16;++slot)assert(store.save(slot,preset));
    edit::Preset restored;assert(store.load(0,restored) && restored.sequence==1);
    preset.values[edit::Depth]=0.5f;assert(store.save(0,preset));assert(store.load(0,restored) && restored.sequence==2);
    store.bytes[0][1][30]^=1;assert(store.load(0,restored) && restored.sequence==1 && restored.values[edit::Depth]==0.75f);
    store.fail=true;assert(!store.save(0,preset));assert(store.load(0,restored) && restored.sequence==1);store.fail=false;
    store.torn=true;assert(!store.save(0,preset));assert(store.load(0,restored) && restored.sequence==1);store.torn=false;
    assert(!store.save(16,preset));
    uint8_t encoded[128];assert(edit::encode(preset,encoded));
    {auto ram_only=preset;ram_only.mask=1u<<edit::Decay;ram_only.values[edit::Decay]=100;assert(!edit::encode(ram_only,encoded));}
    assert(edit::encode(preset,encoded));encoded[4]=2;edit::put32(encoded+124,edit::crc(encoded,124));assert(!edit::decode(encoded,128,restored));
    preset.patch=32;preset.family=1;preset.mask=1;preset.values[0]=1000;assert(!edit::encode(preset,encoded));
    LocalAmyPerformanceBackend backend(runtime.commands());backend.presetStore(&store);backend.publish(runtime.snapshot(),false,0,0);
    assert(backend.editAction(1));backend.service(21);tick();backend.publish(runtime.snapshot(),false,0,0);
    assert(backend.editAction(3,1));tick();runtime.loadPendingPatch();backend.publish(runtime.snapshot(),false,0,0);backend.service(42);tick();backend.publish(runtime.snapshot(),false,0,0);
    assert(runtime.snapshot().edit.mask==(1u<<edit::Depth));
    assert(backend.editParameter(edit::Depth,0.25f));backend.service(100);
    backend.service(3000);assert(backend.editError() && !backend.editPending());
    tick();backend.publish(runtime.snapshot(),false,0,0);
    assert(backend.editParameter(edit::Depth,0.4f));backend.service(3025);tick();backend.publish(runtime.snapshot(),false,0,0);
    assert(!backend.editError() && runtime.snapshot().edit.applied[edit::Depth]==0.4f);
    for(unsigned i=0;i<1000;++i)assert(backend.editParameter(edit::Depth,(i%100)/100.0f));
    assert(backend.editCoalesced()>=999 && runtime.commands().depth()==0);
    backend.service(3050);tick();backend.publish(runtime.snapshot(),false,0,0);assert(runtime.snapshot().edit.applied[edit::Depth]==0.99f);
    std::puts("PASS editor: eight-voice Juno/FM, native maps, no retrigger/allocation, limits, Compare/Revert, stale rejection;16-slot codec/CRC/future version/interrupted overwrite/recall");
    runtime.end();
}

