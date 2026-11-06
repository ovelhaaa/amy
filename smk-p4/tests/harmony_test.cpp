#include "harmony.h"
#include "output_context.h"
#include "pcm_handoff.h"
#include <cassert>
#include <cstdio>
using namespace smk::p4;
int main(){
    const uint8_t scale_notes[11][12]={
        {0,1,2,3,4,5,6,7,8,9,10,11},{0,2,4,5,7,9,11},{0,2,3,5,7,8,10},
        {0,2,3,5,7,8,11},{0,2,4,7,9},{0,3,5,7,10},{0,3,5,6,7,10},
        {0,2,3,5,7,9,10},{0,1,3,5,7,8,10},{0,2,4,6,7,9,11},{0,2,4,5,7,9,10}};
    const unsigned scale_sizes[]={12,7,7,7,5,5,6,7,7,7,7};
    for(unsigned i=0;i<11;++i){uint16_t mask=0;for(unsigned j=0;j<scale_sizes[i];++j)mask|=uint16_t(1u<<scale_notes[i][j]);assert(mask==harmony::kScales[i]);}
    harmony::Config c;assert(harmony::pack(harmony::unpack(harmony::pack(c)))==harmony::pack(c));
    for(unsigned scale=0;scale<11;++scale)for(unsigned root=0;root<12;++root)for(unsigned quant=0;quant<6;++quant)for(unsigned n=0;n<128;++n){
        c.scale=scale;c.root=root;c.quant=quant;c.scale_on=false;
        auto r=harmony::transform(n,c);assert(r.count==1 && r.notes[0]==n);
        c.scale_on=true;r=harmony::transform(n,c);
        if(quant==3 && !r.in_scale){assert(!r.count);continue;}
        assert(r.count==1 && r.notes[0]<128 && (r.mask&(1u<<(r.notes[0]%12))));
        if(r.in_scale)assert(r.notes[0]==n);
        if(quant==4 && r.notes[0]>n)for(unsigned j=0;j<n;++j)assert(!(r.mask&(1u<<(j%12))));
        if(quant==5 && r.notes[0]<n)for(unsigned j=n+1;j<128;++j)assert(!(r.mask&(1u<<(j%12))));
    }
    c={};c.scale_on=true;c.quant=0;assert(harmony::transform(66,c).notes[0]==67);
    c.quant=1;assert(harmony::transform(66,c).notes[0]==65);
    const uint8_t expected[9][4]={{60,64,67},{60,63,67},{60,63,66},{60,64,68},{60,62,67},{60,65,67},{60,64,67,70},{60,64,67,71},{60,63,67,70}};
    c={};c.chord_on=true;
    for(unsigned chord=1;chord<=9;++chord){c.chord=chord;auto r=harmony::transform(60,c);assert(r.count==(chord>=7?4:3));for(unsigned i=0;i<r.count;++i)assert(r.notes[i]==expected[chord-1][i]);}
    c=harmony::preset(1);const uint8_t roots[]={60,62,64,65,67,69,71};
    for(unsigned i=0;i<7;++i){auto r=harmony::transform(roots[i],c);assert(r.count==3 && r.notes[1]==roots[(i+2)%7]+(i+2>=7?12:0) && r.notes[2]==roots[(i+4)%7]+(i+4>=7?12:0));}
    for(unsigned scale=0;scale<11;++scale)if(scale_sizes[scale]==7)for(unsigned key=0;key<12;++key)for(unsigned degree=0;degree<7;++degree){
        c=harmony::preset(3);c.root=key;c.scale=scale;
        const auto r=harmony::transform(uint8_t(48+key+scale_notes[scale][degree]),c);assert(r.count==4);
        for(unsigned voice=0;voice<4;++voice){const unsigned d=degree+2*voice;assert(r.notes[voice]==48+key+scale_notes[scale][d%7]+12*(d/7));}
    }
    c={};c.chord_on=true;c.inversion=1;auto inv=harmony::transform(60,c);assert(inv.notes[0]==64 && inv.notes[1]==67 && inv.notes[2]==72);
    c.inversion=2;inv=harmony::transform(60,c);assert(inv.notes[0]==67 && inv.notes[1]==72 && inv.notes[2]==76);
    c.inversion=3;c.chord=8;inv=harmony::transform(60,c);assert(inv.notes[0]==71 && inv.notes[1]==72 && inv.notes[2]==76 && inv.notes[3]==79);
    c={};c.chord_on=true;c.open=1;inv=harmony::transform(60,c);assert(inv.notes[0]==60 && inv.notes[1]==67 && inv.notes[2]==76);
    c.open=0;inv=harmony::transform(127,c);assert(inv.notes[0]==115 && inv.notes[1]==119 && inv.notes[2]==122);
    c=harmony::preset(2);assert(harmony::transform(69,c).notes[1]==72);
    for(unsigned chord=1;chord<14;++chord)if(chord!=10 && chord!=11)for(unsigned inv=0;inv<(chord>=7?4u:3u);++inv)for(unsigned n=0;n<128;++n){
        c=harmony::preset(1);c.chord=chord;c.inversion=inv;c.open=1;c.spread=2;
        const auto r=harmony::transform(n,c);if(r.error!=harmony::Error::None)continue;
        assert(r.count>=3);for(unsigned i=0;i<r.count;++i){assert(r.notes[i]<=127);if(i)assert(r.notes[i]>r.notes[i-1]);}
    }
    c=harmony::preset(1);c.scale=4;assert(!harmony::valid(c));
    amy::GapCounts g;for(unsigned i=0;i<6;++i)g.add(amy::outputContext(3,amy::OutputReason(i),true),3,false);
    assert(g.boot==1 && g.patch==1 && g.panic==1 && g.unexpected_active==1 && g.other==2);
    g.add(amy::outputContext(3,amy::OutputReason::Ready,true),3,true);assert(g.stale==1);
    g.add(amy::outputContext(2,amy::OutputReason::Ready,true),3,false);assert(g.other==3);
    amy::PcmHandoff<> h;int16_t pcm[512]{};assert(h.push(pcm,1));unsigned stale=0;assert(!h.take(pcm,2,&stale) && stale==1);
    puts("PASS harmony exhaustive scales/keys/MIDI/quantizers, chords/degrees/inversions/range; gap reasons/stale");
}
