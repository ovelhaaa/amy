#pragma once
#include <cstdint>
namespace smk::p4::harmony {
// Blueprint scale order. This is the single P4 table used by preview and owner.
constexpr uint16_t kScales[]={0xfff,0xab5,0x5ad,0x9ad,0x295,0x4a9,0x4e9,0x6ad,0x5ab,0xad5,0x6b5};
enum class Quant : uint8_t { NearUp, NearDown, Root, Filter, Down, Up };
struct Config {
    bool scale_on=false,chord_on=false,touch=true;
    uint8_t root=0,scale=1,quant=0,chord=1,inversion=0,open=0,spread=0,channel=0;
};
struct State {
    Config configuration;
    uint32_t revision=0,errors=0,rejected=0,admitted=0,identity=0;
    uint8_t notes[4]{},count=0,input=0,voices=0,capacity=8,result=0;
    bool last_admitted=false,available=false,pending=false;
};
constexpr uint32_t pack(Config c) {
    return unsigned(c.scale_on)|(unsigned(c.chord_on)<<1)|(unsigned(c.touch)<<2)|
        (uint32_t(c.root)<<3)|(uint32_t(c.scale)<<7)|(uint32_t(c.quant)<<11)|
        (uint32_t(c.chord)<<14)|(uint32_t(c.inversion)<<18)|(uint32_t(c.open)<<20)|
        (uint32_t(c.spread)<<21)|(uint32_t(c.channel)<<23);
}
constexpr Config unpack(uint32_t v) {
    return {bool(v&1),bool(v&2),bool(v&4),uint8_t((v>>3)&15),uint8_t((v>>7)&15),
        uint8_t((v>>11)&7),uint8_t((v>>14)&15),uint8_t((v>>18)&3),
        uint8_t((v>>20)&1),uint8_t((v>>21)&3),uint8_t((v>>23)&15)};
}
inline unsigned degrees(uint8_t scale) {
    unsigned n=0;if(scale<11)for(unsigned i=0;i<12;++i)n+=(kScales[scale]>>i)&1;
    return n;
}
inline bool valid(Config c) {
    return c.root<12 && c.scale<11 && c.quant<6 && c.chord<14 && c.chord!=10 && c.chord!=11 &&
        (c.inversion<3 || (c.inversion==3 && (c.chord==7 || c.chord==8 || c.chord==9 || c.chord==13)) || !c.chord_on) && c.open<2 && c.spread<3 && c.channel<16 &&
        (!(c.chord_on && c.chord>=12) || degrees(c.scale)==7);
}
enum class Error : uint8_t { None, Configuration, Filtered, NonDiatonic, Range };
struct Notes {
    uint8_t notes[4]{},count=0,quantized=0;
    uint16_t mask=0;bool in_scale=false;Error error=Error::None;
};
inline Notes transform(uint8_t input,Config c) {
    Notes r{};r.quantized=input;
    if(input>127 || !valid(c)) {r.error=Error::Configuration;return r;}
    for(unsigned pc=0;pc<12;++pc)if(kScales[c.scale]&(1u<<((pc+12-c.root)%12)))r.mask|=uint16_t(1u<<pc);
    auto inside=[&](int n){return n>=0 && n<128 && (r.mask&(1u<<(n%12)));};
    r.in_scale=inside(input);int q=input;
    if(c.scale_on && !r.in_scale) {
        if(c.quant==3){r.error=Error::Filtered;return r;}
        if(c.quant==2) {q=(input/12)*12+c.root;if(q>127)q-=12;}
        else {
            int up=-1,down=-1;
            for(int d=1;d<=12;++d) {if(up<0 && inside(input+d))up=input+d;if(down<0 && inside(int(input)-d))down=int(input)-d;}
            if(c.quant==4)q=down>=0?down:up;
            else if(c.quant==5)q=up>=0?up:down;
            else if(down<0)q=up;
            else if(up<0)q=down;
            else q=(up-input<int(input)-down || (up-input==int(input)-down && c.quant==0))?up:down;
        }
    }
    r.quantized=uint8_t(q);int ns[4]={q,0,0,0};unsigned count=1;
    if(c.chord_on && c.chord) {
        if(c.chord>=12) {
            if(!inside(q)){r.error=Error::NonDiatonic;return r;}
            count=c.chord==12?3:4;int n=q;unsigned degree=0,index=1;
            // Seven-note scales only. At most 12 scale steps / 24 semitones.
            for(int step=1;step<=24 && index<count;++step)if(r.mask&(1u<<((n+step)%12))) {
                if(++degree%2==0)ns[index++]=n+step;
            }
        } else {
            constexpr uint8_t intervals[10][4]={{0},{0,4,7},{0,3,7},{0,3,6},{0,4,8},{0,2,7},{0,5,7},{0,4,7,10},{0,4,7,11},{0,3,7,10}};
            count=c.chord>=7?4:3;for(unsigned i=0;i<count;++i)ns[i]=q+intervals[c.chord][i];
        }
        if(c.inversion>=count){r.error=Error::Configuration;return r;}
        for(unsigned i=0;i<c.inversion;++i)ns[i]+=12;
        // Sort before opening the second voice; OPEN adds one octave.
        for(unsigned i=0;i<count;++i)for(unsigned j=i+1;j<count;++j)if(ns[j]<ns[i]){int t=ns[i];ns[i]=ns[j];ns[j]=t;}
        if(count>=3)ns[1]+=12*(c.open+c.spread);
    }
    for(unsigned i=0;i<count;++i)for(unsigned j=i+1;j<count;++j)if(ns[j]<ns[i]){int t=ns[i];ns[i]=ns[j];ns[j]=t;}
    // Whole-group octave folding preserves intervals; never clamp individual tones.
    const int shift=ns[count-1]>127?12*((ns[count-1]-127+11)/12):0;
    for(unsigned i=0;i<count;++i) {
        const int n=ns[i]-shift;
        if(n<0 || n>127){r.count=0;r.error=Error::Range;return r;}
        if(!r.count || r.notes[r.count-1]!=n)r.notes[r.count++]=uint8_t(n);
    }
    return r;
}
inline Config preset(unsigned index,Config c={}) {
    c.scale_on=c.chord_on=false;c.root=0;c.scale=1;c.quant=0;c.chord=12;c.inversion=c.open=c.spread=0;
    if(index>=1 && index<=4)c.scale_on=true;
    if(index>=1 && index<=3)c.chord_on=true;
    if(index==2){c.root=9;c.scale=2;}
    if(index==3)c.chord=13;
    return c;
}
}
