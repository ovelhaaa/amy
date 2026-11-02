#include "self_test.h"
#include <cmath>
namespace smk::p4::amy {
bool selfTest(Runtime& runtime,PcmMetrics (&metrics)[kPatchCount],void (*yield)(),uint32_t (*clock_us)()) {
    if(!runtime.snapshot().ready) return false;
    auto& commands=runtime.commands();
    bool passed=true;
    uint32_t now_ms=0;
    for(size_t p=0;p<kPatchCount;++p) {
        auto& m=metrics[p]; m={};
        commands.panic(); runtime.apply(now_ms);
        commands.push({CommandType::Patch,kPatches[p].id,0,0,0,now_ms}); runtime.apply(now_ms);
        uint32_t tokens[kVoices];
        for(unsigned n=0;n<kVoices;++n) {
            tokens[n]=commands.nextToken();
            if(!commands.push({CommandType::NoteOn,static_cast<uint16_t>(60+n*4),100,0,tokens[n],now_ms})) return false;
        }
        runtime.apply(now_ms);
        double sum=0;
        uint64_t dsp_total_us=0;
        const uint32_t clipped_before=runtime.snapshot().clipped;
        for(unsigned b=0;b<40;++b) {
            const uint32_t start=clock_us?clock_us():0;
            const auto* pcm=runtime.render(); if(!pcm) return false;
            const uint32_t duration=clock_us?clock_us()-start:0;
            dsp_total_us+=duration;
            if(duration>m.dsp_max_us) m.dsp_max_us=duration;
            for(unsigned i=0;i<kBlockFrames*2;++i) {
                const int32_t x=pcm[i]; const uint32_t magnitude=x<0?-x:x;
                if(magnitude>m.peak) m.peak=magnitude;
                sum+=double(x)*x;
            }
            if(yield && b%16==0) yield();
        }
        m.dsp_avg_us=static_cast<uint32_t>(dsp_total_us/40);
        m.rms=std::sqrt(sum/(40*kBlockFrames*2)); m.clipped=runtime.snapshot().clipped-clipped_before;
        for(unsigned n=0;n<kVoices;++n) commands.release(60+n*4,tokens[n]);
        runtime.apply(++now_ms);
        for(unsigned b=0;b<15000;++b) { if(!runtime.render()) return false; if(yield && b%16==0) yield(); }
        m.release=true;
        for(unsigned b=0;b<8;++b) {
            const auto* pcm=runtime.render();
            for(unsigned i=0;i<kBlockFrames*2;++i) if(pcm[i]) m.release=false;
        }
        const auto token=commands.nextToken();
        commands.push({CommandType::NoteOn,60,100,0,token,now_ms}); runtime.apply(now_ms); runtime.render();
        commands.panic(); runtime.apply(++now_ms);
        m.panic=true;
        for(unsigned b=0;b<4;++b) {
            const auto* pcm=runtime.render();
            for(unsigned i=0;i<kBlockFrames*2;++i) if(pcm[i]) m.panic=false;
        }
        passed=passed && m.peak>0 && m.rms>0 && !m.clipped && m.release && m.panic && !runtime.snapshot().oom_count;
    }
    return passed;
}
}
