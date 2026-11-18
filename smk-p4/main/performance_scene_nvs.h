#pragma once

#include "performance_scene_store.h"
#include "nvs.h"
#include <cstdio>

namespace smk::p4 {

class NvsSceneStore final : public scene::Store {
public:
    NvsSceneStore()=default;
    NvsSceneStore(const NvsSceneStore&)=delete;
    NvsSceneStore& operator=(const NvsSceneStore&)=delete;
    ~NvsSceneStore() override { if(handle_) nvs_close(handle_); }

    bool begin() {
        if(handle_) return true;
        const auto result=nvs_open(kNamespace,NVS_READONLY,&handle_);
        initialized_=result==ESP_OK || result==ESP_ERR_NVS_NOT_FOUND;
        return initialized_;
    }

protected:
    seq::ReadResult read(unsigned index,unsigned bank,uint8_t* bytes,unsigned& length) override {
        if(!initialized_) return seq::ReadResult::Error;
        if(!handle_) return seq::ReadResult::Missing;
        char key[8];
        makeKey(key,index,bank);
        size_t size=length;
        const auto result=nvs_get_blob(handle_,key,bytes,&size);
        if(result==ESP_ERR_NVS_NOT_FOUND) return seq::ReadResult::Missing;
        if(result==ESP_ERR_NVS_INVALID_LENGTH) return seq::ReadResult::Incompatible;
        if(result!=ESP_OK) return seq::ReadResult::Error;
        length=unsigned(size);
        return seq::ReadResult::Present;
    }

    bool write(unsigned index,unsigned bank,const uint8_t* bytes,unsigned size) override {
        nvs_stats_t stats{};
        const auto required=126+(size+31)/32+3; // GC page plus blob/index entries.
        if(nvs_get_stats(nullptr,&stats)!=ESP_OK || stats.free_entries<required) return false;
        if(!writable_) {
            if(handle_) nvs_close(handle_);
            handle_=0;
            if(nvs_open(kNamespace,NVS_READWRITE,&handle_)!=ESP_OK) {
                begin();
                return false;
            }
            writable_=true;
        }
        char key[8];
        makeKey(key,index,bank);
        return nvs_set_blob(handle_,key,bytes,size)==ESP_OK && nvs_commit(handle_)==ESP_OK;
    }

private:
    static constexpr char kNamespace[]="scenes017b";
    static void makeKey(char (&key)[8],unsigned index,unsigned bank) {
        std::snprintf(key,sizeof(key),"s%02u%c",index,bank?'b':'a');
    }
    nvs_handle_t handle_=0;
    bool initialized_=false,writable_=false;
};

}
