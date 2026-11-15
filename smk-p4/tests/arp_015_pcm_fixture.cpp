#include "amy_runtime.h"
#include <cassert>
#include <cstdio>
#include <cstring>
extern "C" void my_srand48(uint32_t);

using namespace smk::p4;
static amy::Runtime runtime;

int main(int argc,char** argv) {
    assert(argc==2);
    auto* output=std::fopen(argv[1],"wb");
    assert(output);
    for(uint16_t patch:{4,135}) {
        assert(runtime.begin());
        assert(runtime.commands().push({amy::CommandType::Patch,patch}));
        runtime.apply(0);
        runtime.loadPendingPatch();
        assert(runtime.snapshot().patch_id==patch);
        my_srand48(123);
        int peak=0;
        for(unsigned i=0;i<100;++i) {
            const uint32_t now=i*5;
            const auto event=[now](uint8_t status,uint8_t id,uint8_t value) {
                const uint8_t p[]{uint8_t(status>>4),status,id,value};
                assert(runtime.usbInput().packet(p,now));
            };
            if(i==0) { event(0x90,60,97);event(0x90,64,81); }
            if(i==20) event(0xe0,0,70);
            if(i==25) event(0xb0,64,127);
            if(i==30) { event(0x80,60,0);event(0x90,64,0); }
            if(i==40) event(0xb0,64,0);
            runtime.apply(now);
            const int16_t* pcm=runtime.render();
            assert(pcm);
            for(unsigned j=0;j<512;++j) if(pcm[j]>peak) peak=pcm[j];
            assert(std::fwrite(pcm,sizeof(*pcm),512,output)==512);
        }
        assert(peak && runtime.snapshot().active_notes==0);
        runtime.end();
    }
    assert(std::fclose(output)==0);
}
