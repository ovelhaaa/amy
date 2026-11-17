#pragma once
#include "pattern_library.h"
#include "nvs.h"
#include <cstdio>

namespace smk::p4 {

class NvsPatternStore final : public seq::PatternStore {
public:
    NvsPatternStore()=default;
    NvsPatternStore(const NvsPatternStore&)=delete;
    NvsPatternStore& operator=(const NvsPatternStore&)=delete;
    ~NvsPatternStore() override { if(handle_) nvs_close(handle_); }

    bool begin() {
        if(handle_) return true;
        const auto error=nvs_open("patterns017",NVS_READONLY,&handle_);
        return error==ESP_OK || error==ESP_ERR_NVS_NOT_FOUND;
    }

protected:
    seq::ReadResult read(unsigned index,unsigned bank,uint8_t* bytes,unsigned& length) override {
        if(!handle_) return seq::ReadResult::Missing;
        char key[8];
        makeKey(key,index,bank);
        size_t size=length;
        const auto error=nvs_get_blob(handle_,key,bytes,&size);
        if(error==ESP_ERR_NVS_NOT_FOUND) return seq::ReadResult::Missing;
        // Oversize blobs may belong to a future format. Preserve both banks.
        if(error==ESP_ERR_NVS_INVALID_LENGTH) return seq::ReadResult::Incompatible;
        if(error!=ESP_OK) return seq::ReadResult::Error;
        length=unsigned(size);
        return seq::ReadResult::Present;
    }

    bool write(unsigned index,unsigned bank,const uint8_t* bytes) override {
        // Same GC-page reserve as the V4 preset store; three extra entries cover
        // the blob index and split chunk headers.
        constexpr size_t required=126+(seq::kPatternRecordBytes+31)/32+3;
        nvs_stats_t stats{};
        if(nvs_get_stats(nullptr,&stats)!=ESP_OK || stats.free_entries<required)
            return false;
        if(!writable_) {
            if(handle_) nvs_close(handle_);
            handle_=0;
            if(nvs_open("patterns017",NVS_READWRITE,&handle_)!=ESP_OK) {
                begin();
                return false;
            }
            writable_=true;
        }
        char key[8];
        makeKey(key,index,bank);
        return nvs_set_blob(handle_,key,bytes,seq::kPatternRecordBytes)==ESP_OK &&
            nvs_commit(handle_)==ESP_OK;
    }

private:
    static void makeKey(char (&key)[8],unsigned index,unsigned bank) {
        std::snprintf(key,sizeof(key),"p%02u%c",index,bank?'b':'a');
    }
    nvs_handle_t handle_=0;
    bool writable_=false;
};

}
