#include "amy_runtime.h"
#include "user_presets_v2.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
extern "C" {
#include "amy.h"
void my_srand48(uint32_t);void probe_realtime(bool);}
using namespace smk::p4;
static amy::Runtime runtime;
static std::vector<int16_t> run(unsigned patch,unsigned control,bool restore=false,unsigned voices=1,unsigned note=60){
    assert(runtime.begin());assert(runtime.commands().push({amy::CommandType::Patch,uint16_t(patch)}));runtime.apply(0);runtime.loadPendingPatch();
    if((control==10 || control==11) && !(runtime.snapshot().edit.available & (1u<<(control==10?edit::Decay:edit::Sustain)))) {
        std::printf("PCM patch=%u control=%u unavailable by topology/range\n",patch,control);runtime.end();return {};
    }
    if(control && restore){
        edit::Request r;r.patch=patch;r.generation=runtime.snapshot().generation;r.request=1;const unsigned param=control==10?edit::Decay:control==11?edit::Sustain:control==12?edit::FilterEnv:control==13?edit::FilterKeytrack:edit::Cutoff;r.mask=1u<<param;r.values[param]=control==10?1500:control==11?10:control==12?3:control==13?200:500;
        assert(runtime.commands().edits.push(r));runtime.apply(0);assert(runtime.snapshot().edit.errors==0);r.request=2;r.compare=restore;
        if(control==9 || control==11 || control==13){r.compare=false;r.mask=0;}
        assert(runtime.commands().edits.push(r));runtime.apply(0);
    }
    my_srand48(123);
    for(unsigned i=0;i<voices;++i){const uint8_t p[]={9,0x90,uint8_t(note+i),100};assert(runtime.usbInput().packet(p,0));}
    runtime.apply(0);std::vector<int16_t> pcm;pcm.reserve(128*512);
    for(unsigned block=0;block<128;++block){
        if(block==32 && control && !restore){
            edit::Request r;r.patch=patch;r.generation=runtime.snapshot().generation;r.request=1;
            const unsigned parameter=control==1?edit::Cutoff:control==2?edit::Resonance:control==3?edit::Attack:control==4?edit::Release:control==5?edit::Depth:control==6?edit::OpLevel0+1:control==7?edit::OpRatio0+1:control==10?edit::Decay:control==11?edit::Sustain:control==12?edit::FilterEnv:edit::FilterKeytrack;
            r.mask=1u<<parameter;r.values[parameter]=control==1?500:control==2?1.8f:control==3?750:control==4?1200:control==5?0.5f:control==6?50:control==7?2:control==10?1500:control==11?10:control==12?3:200;
            assert(runtime.commands().edits.push(r));
        }
        if(block==96){for(unsigned i=0;i<voices;++i){const uint8_t p[]={9,0x90,uint8_t(note+i),100};assert(runtime.usbInput().packet(p,0));}}
        if(block==64 || block==127){for(unsigned i=0;i<voices;++i){const uint8_t p[]={8,0x80,uint8_t(note+i),0};assert(runtime.usbInput().packet(p,0));}}
        probe_realtime(true);runtime.apply(block*5);assert(runtime.snapshot().edit.errors==0);const int16_t* out=runtime.render();probe_realtime(false);assert(out);pcm.insert(pcm.end(),out,out+512);
    }
    assert(!runtime.snapshot().active_notes);std::printf("PCM patch=%u control=%u voices=%u clipped=%u\n",patch,control,voices,runtime.snapshot().clipped);
    char name[128];std::snprintf(name,sizeof(name),"build/editor011/audio_%u_%u_%u.pcm",patch,control,voices);FILE* file=std::fopen(name,"wb");assert(file);assert(std::fwrite(pcm.data(),sizeof(int16_t),pcm.size(),file)==pcm.size());std::fclose(file);
    runtime.end();return pcm;
}
static std::vector<int16_t> renderPreset(const edit::Preset& preset, const fx::FxConfig& effects) {
    assert(runtime.begin());
    assert(runtime.commands().push({amy::CommandType::Patch, preset.patch}));
    runtime.apply(0);
    runtime.loadPendingPatch();
    edit::Request request;
    request.patch = preset.patch;
    request.generation = runtime.snapshot().generation;
    request.request = 1;
    request.mask = preset.mask;
    std::memcpy(request.macros, preset.macros, sizeof(request.macros));
    std::memcpy(request.values, preset.values, sizeof(request.values));
    request.performance_macros = preset.performance_macros;
    request.performance_routes = preset.performance_routes;
    assert(runtime.commands().edits.push(request));
    fx::FxRequest fx_request;
    fx_request.config = effects;
    fx_request.generation = request.generation;
    fx_request.request = 1;
    assert(runtime.commands().fx.push(fx_request));
    runtime.apply(0);
    assert(!runtime.snapshot().edit.errors && runtime.snapshot().edit.request == 1);
    my_srand48(123);
    const uint8_t on[]{9, 0x90, 60, 100};
    const uint8_t off[]{8, 0x80, 60, 0};
    assert(runtime.usbInput().packet(on, 0));
    std::vector<int16_t> result;
    result.reserve(128 * 512);
    for (unsigned block = 0; block < 128; ++block) {
        if (block == 64) assert(runtime.usbInput().packet(off, block * 5));
        probe_realtime(true);
        runtime.apply(block * 5);
        const auto* pcm = runtime.render();
        probe_realtime(false);
        assert(pcm);
        result.insert(result.end(), pcm, pcm + 512);
    }
    assert(!runtime.snapshot().active_notes);
    runtime.end();
    return result;
}

static void testPresetPcm() {
    edit::Preset fixture;
    fixture.sequence = 1;
    fixture.patch = 4;
    fixture.family = 1;
    std::strcpy(fixture.name, "LEGACY PCM");
    fx::FxConfig effects;
    const auto neutral_pcm = renderPreset(fixture, effects);
    for (unsigned i = 0; i < 4; ++i) {
        auto source = fixture;
        if (i) source.macros[i - 1] = 100;
        const auto expected = renderPreset(source, effects);
        if (i) assert(expected != neutral_pcm);
        edit::Preset decoded;
        uint8_t v1[edit::kRecordBytes];
        assert(edit::encode(source, v1) && edit::decode(v1, sizeof(v1), decoded));
        assert(renderPreset(decoded, effects) == expected);
        uint8_t v2[edit::kV2RecordBytes];
        assert(edit::encodeV2(source, v2) && edit::decodeV2(v2, sizeof(v2), decoded));
        assert(renderPreset(decoded, effects) == expected);
    }
    fixture.patch = 135;
    fixture.family = 0;
    fixture.macros[0] = 44;
    fixture.performance_macros[1] = 50;
    fixture.performance_macros[6] = 96;
    fixture.mask = edit::bit(edit::FmAlgorithm);
    fixture.values[edit::FmAlgorithm] = 7;
    effects.reverb_mix = 0.23f;
    uint8_t v3[edit::kV3RecordBytes];
    edit::Preset decoded;
    assert(edit::encodeV3(fixture, v3) && edit::decodeV3(v3, sizeof(v3), decoded));
    assert(renderPreset(decoded, effects) == renderPreset(fixture, effects));
    fixture.performance_routes = {performance::kFactoryRoutes, true};
    fixture.performance_routes.routes[6][0].depth = 0.52f;
    const unsigned length = edit::v4Size(fixture.performance_routes);
    uint8_t v4[edit::kV4MaxBytes];
    assert(edit::encodeV4(fixture, v4, length) && edit::decodeV4(v4, length, decoded));
    assert(renderPreset(decoded, effects) == renderPreset(fixture, effects));
    std::puts("PASS PCM V1/V2 legacy offsets, V3 FM algorithm and V4 routes after codec recovery");
}

int main(){std::setvbuf(stdout,nullptr,_IONBF,0);
    for(unsigned voices:{1u,8u}){
        auto original=run(4,0,false,voices);
        for(unsigned control:{1u,2u,3u,4u,10u,11u,12u,13u})assert(run(4,control,false,voices)!=original);
        assert(run(4,8,true,voices)==original);assert(run(4,9,true,voices)==original);
        assert(run(4,10,true,voices)==original);assert(run(4,11,true,voices)==original);
        assert(run(4,12,true,voices)==original);assert(run(4,13,true,voices)==original);
        for(unsigned patch:{0u,1u,2u,3u,8u,31u}){auto base=run(patch,0,false,voices);for(unsigned control:{10u,11u}){auto changed=run(patch,control,false,voices);if(!changed.empty())assert(changed!=base);}}
        auto fm=run(135,0,false,voices);
        for(unsigned control:{5u,6u,7u})assert(run(135,control,false,voices)!=fm);
    }
    for(unsigned note:{36u,60u,84u}){
        auto baseline=run(4,0,false,1,note);
        for(unsigned control:{12u,13u}){
            assert(run(4,control,false,1,note)!=baseline);
            assert(run(4,control,true,1,note)==baseline);
        }
    }
    testPresetPcm();
    std::puts("PASS C2/C4/C6 filter PCM change and Compare/Revert restoration");
    std::puts("PASS real AMY PCM changes for compatible Juno decay/sustain and existing controls; Compare/Revert byte-identical fresh note;1/8 voices");
}
