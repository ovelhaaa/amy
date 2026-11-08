#include "local_performance_backend.h"
#include "user_preset_nvs.h"
#include "fake_nvs.h"
#include <cassert>
#include <cstdio>
#include <vector>
#include <algorithm>
extern "C" {
#include "amy.h"
void my_srand48(uint32_t);void probe_realtime(bool);uint32_t probe_calls();
}
using namespace smk::p4;
static amy::Runtime runtime;
static unsigned exercised[edit::Count]{};
enum Mode {Original,Edited,Compared,Reverted,Recalled};
static void midi(uint8_t status,uint8_t a,uint8_t b=0){const uint8_t packet[]={uint8_t(status>>4),status,a,b};assert(runtime.usbInput().packet(packet,0));}
static std::vector<int16_t> render(unsigned param,unsigned note,bool harmony_on,Mode mode,unsigned patch,uint8_t velocity=64){
    assert(runtime.begin());assert(runtime.commands().push({amy::CommandType::Patch,uint16_t(patch)}));runtime.apply(0);runtime.loadPendingPatch();
    NvsUserPresets store;assert(store.begin());LocalAmyPerformanceBackend backend(runtime.commands());backend.presetStore(&store);
    auto publish=[&]{backend.publish(runtime.snapshot(),false,0,0);};publish();
    unsigned now=20;
    auto flush=[&]{backend.service(now);runtime.apply(now);publish();now+=20;assert(!backend.editError());};
    if(!(runtime.snapshot().edit.available&(1u<<param))){runtime.end();return {};}
    const float native=runtime.snapshot().edit.original[param];
    const float value=param==edit::OscLevel?35:param==edit::SubLevel || param==edit::NoiseLevel?(native>20?native*0.4f:native+15):
        param==edit::PulseWidth?(native==30?60:30):param==edit::Detune?35:param==edit::PatchLevel?50:param==edit::Pan?-48:(native==30?80:30);
    struct Native {float amp[NUM_COMBO_COEFS],freq[NUM_COMBO_COEFS],duty[NUM_COMBO_COEFS],pan[NUM_COMBO_COEFS],filter[NUM_COMBO_COEFS];uint16_t wave,chain,mod;};
    Native original[64]{};
    for(unsigned i=0;i<64;++i)if(synth[i]){auto& n=original[i];auto* osc=synth[i];
        std::memcpy(n.amp,osc->amp_coefs,sizeof(n.amp));std::memcpy(n.freq,osc->logfreq_coefs,sizeof(n.freq));
        std::memcpy(n.duty,osc->duty_coefs,sizeof(n.duty));std::memcpy(n.pan,osc->pan_coefs,sizeof(n.pan));
        std::memcpy(n.filter,osc->filter_logfreq_coefs,sizeof(n.filter));n.wave=osc->wave;n.chain=osc->chained_osc;n.mod=osc->mod_source;
    }
    if(mode==Recalled){
        assert(backend.editAction(3,0));runtime.apply(now);runtime.loadPendingPatch();publish();flush();
        assert(runtime.snapshot().edit.applied[param]==value);
    } else if(mode!=Original){
        assert(backend.editParameter(param,value));flush();
        if(mode==Compared){assert(backend.editAction(0));flush();}
        if(mode==Reverted){assert(backend.editAction(1));flush();}
        if(mode==Edited){
            assert(backend.editAction(2,0,"GATE C COMPONENTS"));runtime.apply(now);publish();
            assert(runtime.snapshot().storage_quiet);backend.service(now);runtime.apply(now);publish();now+=20;
            assert(!backend.editError());edit::Preset saved;assert(store.load(0,saved));assert(saved.values[param]==value);
        }
    }
    const bool active=mode==Edited || mode==Recalled;
    for(unsigned v=1;v<=8;++v)assert(instrument_get_level(v)==(active && param==edit::PatchLevel?0.5f:1.0f));
    assert(runtime.snapshot().master==64); // Instrument gain is not Master.
    for(unsigned i=0;i<64;++i)if(synth[i]){
        const auto& n=original[i];const auto* osc=synth[i];
        assert(osc->wave==n.wave && osc->chained_osc==n.chain && osc->mod_source==n.mod);
        assert(!std::memcmp(n.filter,osc->filter_logfreq_coefs,sizeof(n.filter)));
        for(unsigned c=0;c<NUM_COMBO_COEFS;++c){
            if(!active || (!(c==COEF_CONST && (param==edit::OscLevel || param==edit::SubLevel || param==edit::NoiseLevel)) && !(c==COEF_VEL && param==edit::Velocity)))assert(n.amp[c]==osc->amp_coefs[c]);
            if(!active || param!=edit::Detune || c!=COEF_CONST)assert(n.freq[c]==osc->logfreq_coefs[c]);
            if(!active || param!=edit::PulseWidth || c!=COEF_CONST)assert(n.duty[c]==osc->duty_coefs[c]);
            if(!active || param!=edit::Pan || c!=COEF_CONST)assert(n.pan[c]==osc->pan_coefs[c]);
        }
        if(osc->role==SYNTH_IS_MOD_SOURCE){
            assert(!std::memcmp(n.amp,osc->amp_coefs,sizeof(n.amp)) && !std::memcmp(n.freq,osc->logfreq_coefs,sizeof(n.freq)));
            assert(!std::memcmp(n.duty,osc->duty_coefs,sizeof(n.duty)) && !std::memcmp(n.pan,osc->pan_coefs,sizeof(n.pan)));
        }
    }
    if(harmony_on){auto c=harmony::preset(1);c.chord=13;assert(runtime.commands().push({amy::CommandType::Harmony,0,0,0,harmony::pack(c)}));runtime.apply(now);}
    my_srand48(123);midi(0x90,uint8_t(note),velocity);if(harmony_on)midi(0x90,uint8_t(note+2),velocity);
    runtime.apply(now);assert(runtime.snapshot().active_notes==(harmony_on?8u:1u));
    std::vector<int16_t> pcm;pcm.reserve(128*512);const auto allocations=probe_calls();
    for(unsigned block=0;block<128;++block){
        if(block==60)midi(0xb0,64,127);
        if(block==64){midi(0x80,uint8_t(note));if(harmony_on)midi(0x80,uint8_t(note+2));}
        if(block==80)midi(0xb0,64,0);
        if(block==96){midi(0x90,uint8_t(note),velocity);if(harmony_on)midi(0x90,uint8_t(note+2),velocity);}
        if(block==120){midi(0x80,uint8_t(note));if(harmony_on)midi(0x80,uint8_t(note+2));}
        uint32_t clocks[64]{};
        if(block==32 && mode==Edited){
            for(unsigned i=0;i<64;++i)if(synth[i])clocks[i]=synth[i]->note_on_clock;
            const auto& state=runtime.snapshot();edit::Request req;req.patch=state.patch_id;req.generation=state.generation;
            req.mask=state.edit.mask;req.request=state.edit.request+1;
            for(unsigned i=0;i<edit::Count;++i)req.values[i]=state.edit.manual[i];
            assert(runtime.commands().edits.push(req));
        }
        probe_realtime(true);runtime.apply(now+block*5);
        if(block==32 && mode==Edited){
            assert(runtime.snapshot().active_notes==(harmony_on?8u:1u));
            for(unsigned i=0;i<64;++i)if(synth[i])assert(synth[i]->note_on_clock==clocks[i]);
        }
        auto out=runtime.render();probe_realtime(false);assert(out);
        if(block==64)assert(runtime.snapshot().active_notes==(harmony_on?8u:1u));
        pcm.insert(pcm.end(),out,out+512);
    }
    assert(probe_calls()==allocations && !runtime.snapshot().active_notes && !runtime.snapshot().clipped && !runtime.snapshot().edit.errors);
    std::printf("GATE_C patch=%u param=%u note=%u voices=%u mode=%u PCM=%zu clipping=0 render_allocations=0\n",patch,param,note,harmony_on?8:1,unsigned(mode),pcm.size());
    if(param==edit::PatchLevel && mode==Edited){
        assert(runtime.commands().push({amy::CommandType::Patch,128}));runtime.apply(now+1000);runtime.loadPendingPatch();
        assert(runtime.snapshot().patch_state==amy::PatchState::Ready);
        for(unsigned v=1;v<=8;++v)assert(instrument_get_level(v)==1.0f);
    }
    runtime.end();return pcm;
}
static void graphTests(){
    using W=edit::ComponentWave;edit::ComponentNode base[6];
    base[0].wave=W::Control;base[0].filtered=true;base[0].next=2;
    base[1].modulator=true;
    base[2].wave=W::Pulse;base[2].note=1;base[2].next=3;
    base[3].wave=W::Saw;base[3].note=1;base[3].next=4;
    base[4].wave=W::Pulse;base[4].note=1;base[4].logfreq=-1;base[4].next=5;
    base[5].wave=W::Noise;
    unsigned order[6]{0,1,2,3,4,5},count=0;
    do {
        unsigned pos[6];for(unsigned i=0;i<6;++i)pos[order[i]]=i;
        edit::ComponentNode nodes[6];for(unsigned i=0;i<6;++i){nodes[i]=base[order[i]];if(nodes[i].next>=0)nodes[i].next=int8_t(pos[unsigned(nodes[i].next)]);}
        auto map=edit::identifyJuno(nodes,6);assert(map.root==int(pos[0]) && map.pulse==int(pos[2]) && map.sub==int(pos[4]) && map.noise==int(pos[5]));
        assert(map.main_mask==((1u<<pos[2])|(1u<<pos[3])));++count;
    }while(std::next_permutation(order,order+6));assert(count==720);
    auto missing=base[3];base[3].next=5;assert(edit::identifyJuno(base,6).sub==-1);base[3]=missing;
    base[4].next=-1;assert(edit::identifyJuno(base,6).noise==-1);base[4].next=5;
    base[4].logfreq=-0.5f;assert(edit::identifyJuno(base,6).root==-1);base[4].logfreq=-1;
    base[5].next=0;assert(edit::identifyJuno(base,6).root==-1);base[5].next=-1;
    base[0].next=1;assert(edit::identifyJuno(base,6).root==-1);
    std::puts("PASS Juno graph: 720 layouts, absent sub/noise, ambiguous pitch, cycle, modulation exclusion");
}
static std::vector<int16_t> heldEdit(unsigned param,bool change){
    const unsigned patch=param==edit::PulseWidth?31:4;
    assert(runtime.begin());assert(runtime.commands().push({amy::CommandType::Patch,uint16_t(patch)}));runtime.apply(0);runtime.loadPendingPatch();
    auto c=harmony::preset(1);c.chord=13;assert(runtime.commands().push({amy::CommandType::Harmony,0,0,0,harmony::pack(c)}));runtime.apply(0);
    my_srand48(123);midi(0x90,60,64);midi(0x90,62,64);runtime.apply(0);assert(runtime.snapshot().active_notes==8);
    for(unsigned block=0;block<16;++block)runtime.render();
    uint32_t clocks[64]{};for(unsigned i=0;i<64;++i)if(synth[i])clocks[i]=synth[i]->note_on_clock;
    const auto group=runtime.snapshot().last_group;
    if(change){
        const auto& state=runtime.snapshot();edit::Request req;req.patch=uint16_t(patch);req.generation=state.generation;req.request=1;req.mask=1u<<param;
        const float native=state.edit.original[param];
        req.values[param]=param==edit::OscLevel?35:param==edit::SubLevel || param==edit::NoiseLevel?(native>20?native*0.4f:native+15):
            param==edit::PulseWidth?30:param==edit::Detune?35:param==edit::PatchLevel?50:param==edit::Pan?-48:30;
        assert(runtime.commands().edits.push(req));
    }
    std::vector<int16_t> pcm;pcm.reserve(32*512);
    for(unsigned block=0;block<32;++block){
        probe_realtime(true);runtime.apply(100);const auto* out=runtime.render();probe_realtime(false);
        for(unsigned i=0;i<64;++i)if(synth[i])assert(synth[i]->note_on_clock==clocks[i]);
        assert(runtime.snapshot().last_group==group && runtime.snapshot().active_notes==8 && !runtime.snapshot().edit.errors);
        pcm.insert(pcm.end(),out,out+512);
    }
    runtime.commands().panic();runtime.apply(0);assert(!runtime.snapshot().active_notes);runtime.end();return pcm;
}
static void codecCapacityTest(){
    edit::Preset p;p.sequence=1;p.patch=31;p.family=1;std::strcpy(p.name,"MAX JUNO");unsigned count=0;
    for(unsigned i=0;i<edit::Count;++i)if(i<edit::OpLevel0 || i>=edit::Decay){
        p.mask|=1u<<i;++count;
        p.values[i]=i==edit::Cutoff?500:i==edit::Resonance?1:i==edit::Attack || i==edit::Release || i==edit::Decay?100:
            i==edit::Sustain?50:i==edit::FilterEnv?2:i==edit::FilterKeytrack?150:i==edit::PulseWidth?30:i==edit::Pan?-48:35;
    }
    assert(count==16);uint8_t bytes[edit::kV2RecordBytes];assert(edit::encodeV2(p,bytes));edit::Preset decoded;assert(edit::decodeV2(bytes,sizeof(bytes),decoded) && decoded.mask==p.mask);
    for(unsigned i=0;i<edit::Count;++i)if(p.mask&(1u<<i))assert(decoded.values[i]==p.values[i]);
    p.mask|=1u<<edit::OpLevel0;assert(!edit::encodeV2(p,bytes));
    p.patch=135;p.family=0;p.mask=1u<<edit::OscLevel;assert(!edit::encodeV2(p,bytes));
    std::puts("PASS V2 capacity: all 16 Juno fields roundtrip; cross-family masks rejected");
}
static void gainMuteTest(){
    assert(runtime.begin());assert(runtime.commands().push({amy::CommandType::Patch,4}));runtime.apply(0);runtime.loadPendingPatch();
    edit::Request req;req.patch=4;req.generation=runtime.snapshot().generation;req.request=1;req.mask=1u<<edit::PatchLevel;
    assert(runtime.commands().edits.push(req));runtime.apply(0);
    for(unsigned i=0;i<8;++i)midi(0x90,uint8_t(60+i),127);
    runtime.apply(0);assert(runtime.snapshot().active_notes==8 && runtime.snapshot().master==64);
    probe_realtime(true);
    for(unsigned b=0;b<32;++b){const auto* pcm=runtime.render();for(unsigned i=0;i<512;++i)assert(pcm[i]==0);}
    req.mask=0;req.request=2;assert(runtime.commands().edits.push(req));runtime.apply(0);probe_realtime(false);
    for(unsigned v=1;v<=8;++v)assert(instrument_get_level(v)==1.0f);
    runtime.commands().panic();runtime.apply(0);assert(!runtime.snapshot().active_notes);runtime.end();
    std::puts("PASS instrument gain zero: exact stereo silence with 8 active voices, original gain restored, Master unchanged");
}
static void maximumMixTest(){
    for(unsigned patch:{0u,1u,4u,8u,31u}){
        assert(runtime.begin());assert(runtime.commands().push({amy::CommandType::Patch,uint16_t(patch)}));runtime.apply(0);runtime.loadPendingPatch();
        edit::Request req;req.patch=uint16_t(patch);req.generation=runtime.snapshot().generation;req.request=1;
        for(unsigned p=edit::OscLevel;p<=edit::Velocity;++p)if(runtime.snapshot().edit.available&(1u<<p)){
            req.mask|=1u<<p;req.values[p]=p==edit::PulseWidth?95:p==edit::Pan?0:100;
        }
        assert(runtime.commands().edits.push(req));runtime.apply(0);assert(!runtime.snapshot().edit.errors);
        for(unsigned i=0;i<8;++i)midi(0x90,uint8_t(60+i),127);
        runtime.apply(0);assert(runtime.snapshot().active_notes==8);
        for(unsigned b=0;b<128;++b){probe_realtime(true);runtime.render();probe_realtime(false);}
        std::printf("MEASURE maximum component mix patch=%u velocity=127 voices=8 clipped=%u\n",patch,runtime.snapshot().clipped);
        runtime.commands().panic();runtime.apply(0);assert(!runtime.snapshot().active_notes);runtime.end();
    }
}
int main(){std::setvbuf(stdout,nullptr,_IONBF,0);graphTests();codecCapacityTest();
    for(unsigned patch:{0u,1u,4u,8u,31u})for(unsigned param=edit::OscLevel;param<=edit::Velocity;++param)for(bool harmony_on:{false,true}){
        fake_nvs::blobs.clear();fake_nvs::free_entries=700;
        const auto original=render(param,60,harmony_on,Original,patch);
        if(original.empty()){std::printf("GATE_C patch=%u param=%u contextual N/A\n",patch,param);continue;}
        const auto edited=render(param,60,harmony_on,Edited,patch);assert(edited!=original);++exercised[param];
        assert(render(param,60,harmony_on,Compared,patch)==original);
        assert(render(param,60,harmony_on,Reverted,patch)==original);
        assert(render(param,60,harmony_on,Recalled,patch)==edited);
    }
    for(unsigned param=edit::OscLevel;param<=edit::Velocity;++param){std::printf("GATE_C param=%u qualified_cases=%u\n",param,exercised[param]);assert(exercised[param]>0);}
    fake_nvs::blobs.clear();fake_nvs::free_entries=700;
    assert(render(edit::Velocity,60,false,Original,4,127)==render(edit::Velocity,60,false,Edited,4,127));
    const auto soft=render(edit::Velocity,60,false,Original,4,32),less_sensitive=render(edit::Velocity,60,false,Edited,4,32);
    uint64_t a=0,b=0;for(unsigned i=0;i<soft.size();++i){a+=std::abs(int(soft[i]));b+=std::abs(int(less_sensitive[i]));}assert(b>a);
    // Native instrument gain also works for FM, without touching algorithm operators.
    const auto fm_original=render(edit::PatchLevel,60,true,Original,135),fm_edited=render(edit::PatchLevel,60,true,Edited,135);
    assert(fm_original!=fm_edited);assert(render(edit::PatchLevel,60,true,Reverted,135)==fm_original);
    assert(render(edit::PatchLevel,60,true,Recalled,135)==fm_edited);
    for(unsigned param=edit::OscLevel;param<=edit::Velocity;++param)assert(heldEdit(param,false)!=heldEdit(param,true));
    std::puts("PASS all 8 controls change held-note PCM: 8-voice Harmony identity/clocks unchanged, Panic clears ownership");
    gainMuteTest();maximumMixTest();
    std::puts("PASS Gate C real AMY PCM changes/restoration/V2 Recall, 1/8 voices+Harmony, native coefficients, no render allocation or clipping");
}
