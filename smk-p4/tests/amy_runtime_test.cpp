#include "amy_runtime.h"
#include "self_test.h"
#include <cassert>
#include <cmath>
#include <cstdio>
extern "C" void probe_realtime(bool);
extern "C" uint32_t probe_live();
extern "C" void probe_dump();
using namespace smk::p4::amy;
static double energy(Runtime& r,unsigned blocks) {
    double sum=0; unsigned peak=0; uint64_t count=0;
    for(unsigned b=0;b<blocks;++b) {
        const auto* pcm=r.render(); assert(pcm);
        for(unsigned i=0;i<kBlockFrames*2;++i) { const int x=pcm[i]; sum+=double(x)*x; ++count; if(unsigned(std::abs(x))>peak) peak=std::abs(x); }
    }
    std::printf("PCM blocks=%u peak=%u rms=%.4f\n",blocks,peak,std::sqrt(sum/count));
    return sum;
}
int main() {
    setvbuf(stdout,nullptr,_IONBF,0);
    Runtime r;
    uint32_t tokens[kVoices]{};
    const auto baseline=probe_live();
    assert(!r.begin(44100)); assert(!r.render());
    for(unsigned cycle=0;cycle<3;++cycle) {
        assert(r.begin()); assert(!r.begin()); assert(r.snapshot().ready);
        probe_realtime(true);
        assert(energy(r,4)==0);
        for(auto patch:kPatches) {
            assert(r.commands().push({CommandType::Patch,patch.id,0,0,0,0})); r.apply(0);
            for(unsigned n=0;n<kVoices;++n) { tokens[n]=r.commands().nextToken(); assert(r.commands().push({CommandType::NoteOn,uint16_t(60+n*4),100,0,tokens[n],0})); }
            r.apply(1); assert(r.snapshot().active_notes==kVoices);
            assert(energy(r,40)>0); assert(r.snapshot().clipped==0);
            for(unsigned n=0;n<kVoices;++n) r.commands().release(60+n*4,tokens[n]);
            r.apply(2); assert(!r.snapshot().active_notes);
            assert(energy(r,4)>0); assert(energy(r,15000)>=0); assert(energy(r,8)==0);
        }
        r.commands().panic(); r.apply(3); assert(energy(r,8)==0);
        probe_realtime(false);
        r.end(); assert(!r.render());
        std::printf("HEAP live=%u baseline=%u\n",probe_live(),baseline);
        if(probe_live()!=baseline) probe_dump();
        assert(probe_live()==baseline);
    }
    assert(r.begin());
    PcmMetrics metrics[kPatchCount];
    probe_realtime(true);
    assert(selfTest(r,metrics));
    probe_realtime(false);
    for(size_t p=0;p<kPatchCount;++p) std::printf("RAM_SELF_TEST patch=%u peak=%u rms=%.4f clipped=%u release=%d panic=%d\n",
        kPatches[p].id,metrics[p].peak,metrics[p].rms,metrics[p].clipped,metrics[p].release,metrics[p].panic);
    r.end(); assert(probe_live()==baseline);
    const auto velocity_energy=[&](uint8_t velocity) {
        assert(r.begin());
        const uint32_t token=r.commands().nextToken();
        assert(r.commands().push({CommandType::NoteOn,60,velocity,0,token,0}));
        probe_realtime(true); r.apply(0);
        const double sum=energy(r,40);
        assert(r.snapshot().clipped==0);
        probe_realtime(false); r.end(); return sum;
    };
    const double soft=velocity_energy(30),loud=velocity_energy(100);
    assert(soft>0 && loud>soft && probe_live()==baseline);
    std::puts("PASS: real AMY initialization, ROM128/135, configured notes, release, clipping, Panic and restart");
}
