#include "harmonic_pads.h"
#include <cassert>
#include <cstring>
using namespace smk::p4;
int main() {
    pads::Config c;
    const unsigned roots[]={60,62,64,65,67,69,71,72};
    const char* names[]={"C","Dm","Em","F","G","Am","Bdim","C"};
    for(unsigned i=0;i<8;++i) {
        const auto intent=pads::resolve(i,c);assert(intent.valid && intent.input==roots[i]);
        char text[24];pads::name(intent,text,sizeof(text));assert(!std::strcmp(text,names[i]));
        const auto notes=harmony::transform(intent.input,intent.configuration);assert(notes.count==3);
    }
    c.root=9;c.scale=2;
    const char* minor[]={"Am","Bdim","C","Dm","Em","F","G","Am"};
    for(unsigned i=0;i<8;++i){char text[24];pads::name(pads::resolve(i,c),text,sizeof(text));assert(!std::strcmp(text,minor[i]));}
    for(unsigned scale=0;scale<11;++scale) {
        c.scale=uint8_t(scale);
        for(unsigned i=0;i<8;++i) {
            auto intent=pads::resolve(i,c);
            assert(intent.valid==(harmony::degrees(c.scale)==7));
            if(!intent.valid) continue;
            for(unsigned inv=0;inv<3;++inv) for(unsigned open=0;open<2;++open) {
                intent.configuration.inversion=uint8_t(inv);intent.configuration.open=uint8_t(open);
                const auto notes=harmony::transform(intent.input,intent.configuration);
                assert(notes.error==harmony::Error::None && notes.count==3);
                for(unsigned n=0;n<notes.count;++n)assert(notes.notes[n]<=127);
            }
        }
    }
    c.scale=1;c.root=0;c.octave=9;assert(!pads::resolve(7,c).valid);
    c.octave=8;c.root=7;c.seventh=true;
    auto intent=pads::resolve(6,c);assert(intent.valid);
    auto notes=harmony::transform(intent.input,intent.configuration);assert(notes.count==4);
    assert(!pads::resolve(8,c).valid);
}
