#include "controller_profile_nvs.h"
#include <cassert>
#include <cstring>

using namespace smk::p4;
using namespace smk::p4::performance;

namespace {
uint8_t nvs_bytes[kProfileBytes]{};
size_t nvs_length=0;
bool available=true;
bool corrupt_readback=false;
unsigned writes=0;
}

extern "C" esp_err_t nvs_open(const char*,int,nvs_handle_t* handle) {
    *handle=1;
    return available ? ESP_OK : ESP_FAIL;
}
extern "C" void nvs_close(nvs_handle_t) {}
extern "C" esp_err_t nvs_commit(nvs_handle_t) { return ESP_OK; }
extern "C" esp_err_t nvs_set_blob(nvs_handle_t,const char*,const void* data,size_t size) {
    assert(size==sizeof(nvs_bytes));
    std::memcpy(nvs_bytes,data,size);
    nvs_length=size;
    ++writes;
    return ESP_OK;
}
extern "C" esp_err_t nvs_get_blob(nvs_handle_t,const char*,void* out,size_t* size) {
    if(!nvs_length) return ESP_ERR_NVS_NOT_FOUND;
    if(*size<nvs_length) return ESP_ERR_NVS_INVALID_LENGTH;
    std::memcpy(out,nvs_bytes,nvs_length);
    if(corrupt_readback) static_cast<uint8_t*>(out)[8]^=1;
    *size=nvs_length;
    return ESP_OK;
}

int main() {
    NvsControllerProfile store;
    ControllerProfile decoded;
    assert(!store.load(decoded));
    auto profile=smk25Defaults();
    assert(profile.legacy && validProfile(profile));
    for(unsigned i=0;i<16;++i){
        const MidiSource expected{0,0,uint8_t(21+i)};
        assert(profile.knobs[i]==expected);
    }
    profile.physical_b_destinations[7]=uint8_t(kMidiTargetCount-1);
    profile.destinations[0][4]=2;
    profile.knobs[10]={0,3,76};
    assert(writes==0 && store.save(profile) && writes==1);
    NvsControllerProfile reboot;
    assert(reboot.load(decoded) && decoded.legacy && decoded.knobs==profile.knobs);
    assert(decoded.destinations==profile.destinations);
    assert(decoded.physical_b_destinations==profile.physical_b_destinations);
    profile.knobs[15]=profile.knobs[0];
    assert(!store.save(profile) && writes==1);
    nvs_bytes[88]^=1;
    assert(!reboot.load(decoded) && decoded.legacy);
    nvs_bytes[88]^=1;
    available=false;
    assert(!reboot.load(decoded));
    available=true;
    corrupt_readback=true;
    assert(!store.save(smk25Defaults()));
    corrupt_readback=false;

    // The first 32 bytes are shared by the three wire versions.
    for(unsigned version : {1u,2u}){
        nvs_length=version==1 ? kProfileV1Bytes : kProfileV2Bytes;
        nvs_bytes[4]=uint8_t(version);
        nvs_bytes[5]=3;
        nvs_bytes[7]=0;
        std::memset(nvs_bytes+32,0,32);
        nvs_bytes[32+3*8+5]=37;
        edit::put32(nvs_bytes+nvs_length-4,edit::crc(nvs_bytes,nvs_length-4));
        assert(reboot.load(decoded) && !decoded.legacy && decoded.bank==3);
        assert(decoded.knobs[7].controller==28 && decoded.knobs[8].channel==16);
        assert(decoded.destinations[3][5]==(version==1 ? 0 : 37));
    }

    Pickup a,b;
    uint8_t output=0;
    assert(!a.update(20,64,output));
    assert(!a.update(20,64,output));
    assert(!b.update(90,64,output));
    assert(a.update(70,64,output) && output==64);
    assert(a.update(71,64,output) && output==71);
    assert(b.state()==PickupState::ApproachingFromAbove);
    assert(b.update(64,64,output) && output==64);
    b.reset();
    assert(!b.update(200,64,output) && b.state()==PickupState::Waiting);
}
