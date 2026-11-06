#include "local_performance_backend.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
extern "C" {
#include "amy.h"
size_t probe_bytes();uint32_t probe_live();void probe_realtime(bool);void probe_fail_next(unsigned);void probe_on_allocation(unsigned,void (*)());
}
using namespace smk::p4;
static amy::Runtime* active=nullptr;
static void cancelLoad(){active->commands().panic();}
static void disconnectLoad(){active->usbInput().disconnect();}
static void usb(amy::Runtime& r,uint8_t status,uint8_t a,uint8_t b=0){const uint8_t p[]={uint8_t(status>>4),status,a,b};assert(r.usbInput().transfer(p,4,0));}
static void select(amy::Runtime& r,uint16_t id){assert(r.commands().push({amy::CommandType::Patch,id}));r.apply(0);assert(r.snapshot().patch_state==amy::PatchState::Requested);r.loadPendingPatch();assert(r.snapshot().patch_state==amy::PatchState::Ready && r.snapshot().patch_id==id && !r.snapshot().active_notes);}
int main(){
    static_assert(amy::kAmyVoices==8 && amy::kOscillators==64 && amy::kSynthCount==9);
    const auto baseline=probe_bytes();const auto live=probe_live();amy::Runtime r;active=&r;assert(r.begin());
    size_t stable[256]{};unsigned swaps=0;
    for(unsigned i=0;i<256;++i){auto* p=library::factoryAt(i);assert(p && p->id==i && p->name[0] && p->bank[0]);assert(p->oscillators==(i<128?6:8));assert(p->category!=library::PatchCategory::All && p->category<library::PatchCategory::Count);assert(bool(p->qualification&library::SUPPORTED)==(amy::patchIndex(i)>=0));}
    for(unsigned cycle=0;cycle<3;++cycle)for(unsigned pos=0;pos<256;++pos){
        unsigned id=pos/2+(pos%2?128:0);if(amy::patchIndex(id)<0)continue;
        // Switch with sustained Harmony groups; the patch barrier must clear them.
        auto c=harmony::preset(1);assert(r.commands().push({amy::CommandType::Harmony,0,0,0,harmony::pack(c)}));r.apply(0);
        usb(r,0xb0,64,127);usb(r,0x90,60,100);usb(r,0x90,64,100);r.apply(0);r.render();assert(r.snapshot().active_notes==6);
        usb(r,0x80,60);r.apply(0);assert(r.snapshot().active_notes==6);
        select(r,id);++swaps;
        auto bytes=probe_bytes();if(!cycle)stable[id]=bytes;else assert(stable[id]==bytes);
        usb(r,0x90,60,100);usb(r,0x90,64,100);r.apply(0);assert(r.snapshot().active_notes==6);
        usb(r,0x80,60);r.apply(0);assert(r.snapshot().active_notes==3);usb(r,0x80,64);r.apply(0);assert(!r.snapshot().active_notes);
        assert(r.commands().push({amy::CommandType::Harmony,0,0,0,harmony::pack(harmony::Config{})}));r.apply(0);
        for(unsigned n=0;n<8;++n)usb(r,0x90,48+3*n,100);r.apply(0);assert(r.snapshot().active_notes==8);
        unsigned peak=0;probe_realtime(true);for(unsigned b=0;b<8;++b){const auto* pcm=r.render();for(unsigned j=0;j<512;++j){unsigned a=pcm[j]<0?-int(pcm[j]):pcm[j];if(a>peak)peak=a;}}probe_realtime(false);
        if(!cycle)std::printf("RUNTIME_PATCH id=%u heap=%zu master64_peak=%u active=8\n",id,bytes-baseline,peak);
        r.usbInput().disconnect();r.apply(0);assert(!r.snapshot().active_notes);
        assert(!r.snapshot().oom_count);
    }
    const auto confirmed=r.snapshot().patch_id;
    for(unsigned id=0;id<257;++id)if(amy::patchIndex(id)<0){assert(r.commands().push({amy::CommandType::Patch,uint16_t(id)}));r.apply(0);assert(r.snapshot().patch_id==confirmed && r.snapshot().patch_state==amy::PatchState::Ready);}
    select(r,128);select(r,128);
    LocalAmyPerformanceBackend b(r.commands());b.publish(r.snapshot(),false,0,0);
    const auto other=amy::kPatches[1].id;assert(b.setPatch(other,0));assert(!b.setPatch(128,0));r.apply(0);probe_fail_next(1);r.loadPendingPatch();assert(r.snapshot().patch_state==amy::PatchState::Failed && !r.snapshot().active_notes);
    select(r,128);
    for(auto callback:{cancelLoad,disconnectLoad}){
        assert(r.commands().push({amy::CommandType::Patch,other}));r.apply(0);
        probe_on_allocation(1,callback);r.loadPendingPatch();
        assert(r.snapshot().patch_state==amy::PatchState::Failed && !r.snapshot().active_notes);
        select(r,128);
    }
    r.commands().panic();r.apply(0);assert(!r.snapshot().active_notes);
    r.end();assert(probe_bytes()==baseline && probe_live()==live);
    std::printf("PASS factory Runtime swaps=%u eight voices/64 oscs, all metadata, Harmony/sustain/release/disconnect, invalid/failed IDs, busy, load OOM recovery, no cumulative growth and balanced teardown\n",swaps);
}
