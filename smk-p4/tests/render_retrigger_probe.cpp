// Offline diagnostic only: public Runtime/USB path, no DSP or firmware changes.
#include "amy_runtime.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
using namespace smk::p4::amy;
static void le16(FILE* f,uint16_t x) { std::fputc(x&255,f);std::fputc(x>>8,f); }
static void le32(FILE* f,uint32_t x) { le16(f,uint16_t(x));le16(f,uint16_t(x>>16)); }
struct Probe {
    Runtime runtime; FILE* file=nullptr; uint32_t frames=0;
    int previous[2]{}; int onset_jump=0,off_jump=0,all_delta=0;
    explicit Probe(const char* path,uint16_t patch) {
        assert(runtime.begin()); assert(runtime.snapshot().master==64);
        assert(runtime.commands().push({CommandType::Patch,patch,0,0,0,0}));runtime.apply(0);
        file=std::fopen(path,"wb");assert(file);
        std::fwrite("RIFF",1,4,file);le32(file,0);std::fwrite("WAVEfmt ",1,8,file);
        le32(file,16);le16(file,1);le16(file,2);le32(file,kSampleRateHz);le32(file,kSampleRateHz*4);
        le16(file,4);le16(file,16);std::fwrite("data",1,4,file);le32(file,0);
    }
    uint32_t now() const { return uint32_t(uint64_t(frames)*1000/kSampleRateHz); }
    void midi(bool on,uint8_t note) {
        uint8_t packet[]={uint8_t(on?9:8),uint8_t(on?0x90:0x80),note,uint8_t(on?100:0)};
        assert(runtime.usbInput().transfer(packet,4,now()));
    }
    void blocks(unsigned count,int edge=0) {
        for(unsigned b=0;b<count;++b) {
            runtime.apply(now());const auto* pcm=runtime.render();assert(pcm);
            if(b==0 && edge) for(unsigned c=0;c<2;++c) {
                int jump=std::abs(int(pcm[c])-previous[c]);
                if(edge>0) onset_jump=std::max(onset_jump,jump);else off_jump=std::max(off_jump,jump);
            }
            for(unsigned f=0;f<kBlockFrames;++f) for(unsigned c=0;c<2;++c) {
                int value=pcm[2*f+c];all_delta=std::max(all_delta,std::abs(value-previous[c]));previous[c]=value;
            }
            assert(std::fwrite(pcm,sizeof(int16_t),kBlockFrames*2,file)==kBlockFrames*2);
            frames+=kBlockFrames;
        }
    }
    void finish(uint16_t patch) {
        std::fseek(file,4,SEEK_SET);le32(file,36+frames*4);
        std::fseek(file,40,SEEK_SET);le32(file,frames*4);std::fclose(file);file=nullptr;
        std::printf("patch=%u master=64 frames=%u onset_boundary_jump=%d off_boundary_jump=%d max_sample_delta=%d clipped=%u polyphony_rejected=%u\n",patch,frames,onset_jump,off_jump,all_delta,runtime.snapshot().clipped,runtime.snapshot().polyphony_rejected);
        runtime.end();
    }
};
int main(int argc,char** argv) {
    assert(argc==3); // output directory + prefix; directories created by runner
    for(const auto& patch:kPatches) {
        char path[512];std::snprintf(path,sizeof(path),"%s/%s_patch_%u.wav",argv[1],argv[2],patch.id);
        Probe p(path,patch.id);p.blocks(38);
        // Four isolated C4s, sixteen rapid repeated C4s, rapid changing notes,
        // then eight four-note chords. Same master/velocity and no sustain/FX.
        for(unsigned n=0;n<4;++n) { p.midi(true,60);p.blocks(20,1);p.midi(false,60);p.blocks(225,-1); }
        p.blocks(56);
        for(unsigned n=0;n<16;++n) { p.midi(true,60);p.blocks(12,1);p.midi(false,60);p.blocks(3,-1); }
        p.blocks(56);
        const uint8_t notes[]={60,64,67,72};
        for(unsigned n=0;n<16;++n) { p.midi(true,notes[n%4]);p.blocks(8,1);p.midi(false,notes[n%4]);p.blocks(2,-1); }
        p.blocks(56);
        for(unsigned n=0;n<8;++n) { for(auto note:notes) p.midi(true,note);p.blocks(12,1);for(auto note:notes) p.midi(false,note);p.blocks(3,-1); }
        p.blocks(375);assert(p.runtime.snapshot().active_notes==0);p.finish(patch.id);
    }
}
