#pragma once
#include "fx_state.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace smk::p4::tempo {
class NvsTempo {
public:
    unsigned load() const {
        if(nvs_flash_init()!=ESP_OK) return kDefaultBpm;
        nvs_handle_t handle{};
        if(nvs_open("perf0151tempo",NVS_READONLY,&handle)!=ESP_OK) return kDefaultBpm;
        uint8_t bytes[8]{};
        size_t length=sizeof(bytes);
        const auto error=nvs_get_blob(handle,"global",bytes,&length);
        nvs_close(handle);
        if(error!=ESP_OK || length!=sizeof(bytes) || bytes[0]!='T' ||
           bytes[1]!=1 || bytes[3]!=0 || bytes[2]<kMinBpm || bytes[2]>kMaxBpm) return kDefaultBpm;
        uint8_t expected[8]{};
        encode(bytes[2],expected);
        return std::memcmp(expected,bytes,sizeof(bytes))==0?bytes[2]:kDefaultBpm;
    }
    bool save(unsigned bpm) const {
        if(bpm<kMinBpm || bpm>kMaxBpm || nvs_flash_init()!=ESP_OK) return false;
        uint8_t bytes[8]{};
        encode(bpm,bytes);
        nvs_handle_t handle{};
        if(nvs_open("perf0151tempo",NVS_READWRITE,&handle)!=ESP_OK) return false;
        auto error=nvs_set_blob(handle,"global",bytes,sizeof(bytes));
        if(error==ESP_OK) error=nvs_commit(handle);
        uint8_t check[8]{};
        size_t length=sizeof(check);
        if(error==ESP_OK) error=nvs_get_blob(handle,"global",check,&length);
        nvs_close(handle);
        return error==ESP_OK && length==sizeof(check) && std::memcmp(bytes,check,sizeof(bytes))==0;
    }
private:
    static void encode(unsigned bpm,uint8_t (&bytes)[8]) {
        bytes[0]='T';bytes[1]=1;bytes[2]=uint8_t(bpm);bytes[3]=0;
        const uint32_t crc=fx::calcCrc(bytes,4);
        for(unsigned i=0;i<4;++i) bytes[4+i]=uint8_t(crc>>(8*i));
    }
};
}
