#include "local_performance_backend.h"
#include "user_preset_nvs.h"
#include "user_preset_fx_nvs.h"
#include "fake_nvs.h"
#include <cassert>
#include <cstdio>
#include <vector>
extern "C" {
#include "amy.h"
void my_srand48(uint32_t);void probe_realtime(bool);uint32_t probe_calls();
}
using namespace smk::p4;
static amy::Runtime runtime;
static unsigned eg1_cases=0;
enum Mode {Original,Edited,Compared,Reverted,Recalled};
static void midi(uint8_t status,uint8_t a,uint8_t b=0){const uint8_t packet[]={uint8_t(status>>4),status,a,b};assert(runtime.usbInput().packet(packet,0));}
static std::vector<int16_t> render(unsigned param,unsigned note,bool harmony_on,Mode mode,unsigned patch){
    assert(runtime.begin());assert(runtime.commands().push({amy::CommandType::Patch,uint16_t(patch)}));runtime.apply(0);runtime.loadPendingPatch();
    NvsUserPresets store;assert(store.begin());fx::NvsFxPresets fx_store;assert(fx_store.begin());LocalAmyPerformanceBackend backend(runtime.commands());backend.presetStore(&store);backend.fxPresetStore(&fx_store);
    auto publish=[&]{backend.publish(runtime.snapshot(),false,0,0);};publish();
    unsigned now=20;
    auto flush=[&]{backend.service(now);runtime.apply(now);publish();now+=20;assert(!backend.editError());};
    float original[64][NUM_COMBO_COEFS]{};
    for(unsigned i=0;i<64;++i)if(synth[i])std::memcpy(original[i],synth[i]->filter_logfreq_coefs,sizeof(original[i]));
    const float value=param==edit::FilterEnv?3.0f:150.0f;
    if(mode==Recalled){
        assert(backend.editAction(3,0));runtime.apply(now);runtime.loadPendingPatch();publish();flush();
        assert(runtime.snapshot().edit.applied[param]==value);
    } else if(mode!=Original){
        assert(backend.editParameter(param,value));flush();
        if(mode==Compared){assert(backend.editAction(0));flush();}
        if(mode==Reverted){assert(backend.editAction(1));flush();}
        if(mode==Edited){
            assert(backend.editAction(2,0,"GATE B FILTER"));runtime.apply(now);publish();
            assert(runtime.snapshot().storage_quiet);backend.service(now);runtime.apply(now);publish();now+=20;
            assert(!backend.editError());edit::Preset saved;assert(store.load(0,saved));assert(saved.values[param]==value);
        }
    }
    for(unsigned i=0;i<64;++i)if(synth[i] && synth[i]->filter_type!=FILTER_NONE){
        for(unsigned c=0;c<NUM_COMBO_COEFS;++c){
            const unsigned changed=param==edit::FilterEnv?((original[i][COEF_EG0]==0 && (original[i][COEF_EG1]!=0 || (synth[i]->breakpoint_times[1] && AMY_IS_SET(synth[i]->breakpoint_times[1][0]))))?COEF_EG1:COEF_EG0):COEF_NOTE;
            if(param==edit::FilterEnv && changed==COEF_EG1 && mode==Edited)++eg1_cases;
            const bool active=mode==Edited || mode==Recalled;
            const float expected=active && c==changed?(param==edit::FilterEnv?value:value/100.0f):original[i][c];
            assert(synth[i]->filter_logfreq_coefs[c]==expected);
        }
    }
    if(harmony_on){auto c=harmony::preset(1);c.chord=13;assert(runtime.commands().push({amy::CommandType::Harmony,0,0,0,harmony::pack(c)}));runtime.apply(now);}
    my_srand48(123);midi(0x90,uint8_t(note),100);if(harmony_on)midi(0x90,uint8_t(note+2),100);
    runtime.apply(now);assert(runtime.snapshot().active_notes==(harmony_on?8u:1u));
    std::vector<int16_t> pcm;pcm.reserve(128*512);const auto allocations=probe_calls();
    for(unsigned block=0;block<128;++block){
        if(block==60)midi(0xb0,64,127);
        if(block==64){midi(0x80,uint8_t(note));if(harmony_on)midi(0x80,uint8_t(note+2));}
        if(block==80)midi(0xb0,64,0);
        if(block==96){midi(0x90,uint8_t(note),100);if(harmony_on)midi(0x90,uint8_t(note+2),100);}
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
    std::printf("GATE_B patch=%u param=%u note=%u voices=%u mode=%u PCM=%zu clipping=0 render_allocations=0\n",patch,param,note,harmony_on?8:1,unsigned(mode),pcm.size());
    runtime.end();return pcm;
}
int main(){std::setvbuf(stdout,nullptr,_IONBF,0);
    for(unsigned patch:{0u,1u,4u,8u})for(unsigned param:{unsigned(edit::FilterEnv),unsigned(edit::FilterKeytrack)})for(unsigned note:{36u,60u,84u})for(bool harmony_on:{false,true}){
        fake_nvs::blobs.clear();fake_nvs::free_entries=700;
        const auto original=render(param,note,harmony_on,Original,patch);
        const auto edited=render(param,note,harmony_on,Edited,patch);assert(edited!=original);
        assert(render(param,note,harmony_on,Compared,patch)==original);
        assert(render(param,note,harmony_on,Reverted,patch)==original);
        assert(render(param,note,harmony_on,Recalled,patch)==edited);
    }
    assert(eg1_cases>0);
    std::puts("PASS Gate B real AMY: C2/C4/C6, 1/8 voices+Harmony, native coefficients, PCM/Compare/Revert/Save/Recall+reboot, sustain/retrigger/release, no render allocation or clipping");
}
