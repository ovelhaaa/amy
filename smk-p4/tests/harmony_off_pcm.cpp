#include "amy_runtime.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
using namespace smk::p4;
int main(int argc,char** argv){assert(argc==2);amy::Runtime r;assert(r.begin());
    for(unsigned patch:{128,135,138,142,4,8,31}){
        assert(r.commands().push({amy::CommandType::Patch,uint16_t(patch)}));r.apply(0);r.loadPendingPatch();assert(r.snapshot().patch_id==patch);
        uint8_t p[]={9,0x90,60,100};assert(r.usbInput().transfer(p,4,0));r.apply(0);
        char path[256];std::snprintf(path,sizeof(path),"%s/off_%u.pcm",argv[1],patch);FILE* f=std::fopen(path,"wb");assert(f);
        for(unsigned i=0;i<128;++i){auto pcm=r.render();assert(pcm);assert(std::fwrite(pcm,2,512,f)==512);}assert(!std::fclose(f));
    }
    puts("PASS Harmony OFF PCM rendered");
}
