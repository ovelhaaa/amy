#include "user_preset_nvs_v2.h"
#include "fake_nvs.h"
#include <cassert>
#include <cstdio>
using namespace smk::p4;
int main(){
    edit::Preset p;p.sequence=8;p.patch=4;p.family=1;std::strcpy(p.name,"MY FILTER");
    p.mask=(1u<<edit::FilterEnv)|(1u<<edit::FilterKeytrack);p.values[edit::FilterEnv]=3;p.values[edit::FilterKeytrack]=150;
    edit::Preset old=p;old.mask=1;old.values[0]=700;uint8_t v1[128];assert(edit::encode(old,v1));
    for(unsigned slot=0;slot<16;++slot)for(char bank:{'a','b'}){
        char key[12];std::snprintf(key,sizeof(key),"s%02u%c",slot,bank);fake_nvs::blobs[{1,key}]={v1,v1+128};
    }
    fake_nvs::free_entries=476; // Full legacy banks already consume their entries.
    const auto legacy=fake_nvs::blobs[{1,"s00a"}];
    NvsVersionedPresets store;assert(store.begin());edit::Preset out;assert(store.load(0,out) && out.sequence==8);
    fake_nvs::free_entries=389;const auto before_admission=fake_nvs::writes;
    assert(!store.save(0,p) && fake_nvs::writes==before_admission);assert(store.load(0,out) && out.sequence==8);
    fake_nvs::free_entries=476;
    for(unsigned slot=0;slot<16;++slot){assert(store.save(slot,p));assert(store.save(slot,p));}
    assert(fake_nvs::blobs.size()==64);assert(fake_nvs::blobs.at({1,"s00a"})==legacy);
    NvsVersionedPresets reboot;assert(reboot.begin());assert(reboot.load(0,out) && out.sequence==10 && out.values[edit::FilterEnv]==3);
    fake_nvs::torn=true;assert(!reboot.save(0,p));fake_nvs::torn=false;
    assert(reboot.load(0,out) && out.sequence==10);
    const auto writes=fake_nvs::writes;
    fake_nvs::free_entries=125;assert(!reboot.save(0,p));assert(fake_nvs::writes==writes);assert(reboot.load(0,out));
    fake_nvs::free_entries=500;fake_nvs::read_error=true;assert(!reboot.save(0,p));assert(fake_nvs::writes==writes);fake_nvs::read_error=false;
    fake_nvs::stats_error=true;assert(!reboot.save(0,p));assert(fake_nvs::writes==writes);fake_nvs::stats_error=false;
    fake_nvs::commit_error=true;assert(!reboot.save(0,p));fake_nvs::commit_error=false;assert(reboot.load(0,out));
    fake_nvs::blobs[{2,"v00a"}][30]^=1;assert(reboot.load(0,out));
    assert(fake_nvs::blobs.at({1,"s00a"})==legacy);
    std::puts("PASS NVS v2: 16 dual-bank slots, retained V1, reboot, torn/CRC, read/space/stats/commit errors");
}
