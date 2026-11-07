#pragma once
#include "user_presets.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <cstdio>
namespace smk::p4 {
class NvsUserPresets final : public edit::PresetStore {
public:
    bool begin(){
        // Never erase a partition on init failure: preserve recovery data.
        error_=nvs_flash_init();if(error_!=ESP_OK)return false;
        nvs_stats_t stats{};error_=nvs_get_stats(nullptr,&stats);if(error_!=ESP_OK)return false;
        error_=nvs_open("sound011",NVS_READWRITE,&handle_);if(error_!=ESP_OK)return false;
        size_t used=0;error_=nvs_get_used_entry_count(handle_,&used);if(error_!=ESP_OK)return false;
        // Conservative seven entries/blob and one page of GC margin. Include
        // our existing entries so rebooting a populated store remains admitted.
        if(stats.free_entries+used<edit::kSlots*2*7+126){nvs_close(handle_);handle_=0;error_=ESP_ERR_NVS_NOT_ENOUGH_SPACE;return false;}
        return true;
    }
    bool read(unsigned slot,unsigned bank,uint8_t* data,unsigned& length) override {
        if(!handle_ || slot>=edit::kSlots || bank>1)return false;
        char key[12];std::snprintf(key,sizeof(key),"s%02u%c",slot,bank?'b':'a');
        size_t n=length;error_=nvs_get_blob(handle_,key,data,&n);length=unsigned(n);return error_==ESP_OK;
    }
    bool write(unsigned slot,unsigned bank,const uint8_t* data,unsigned length) override {
        if(!handle_ || slot>=edit::kSlots || bank>1 || length!=edit::kRecordBytes)return false;
        char key[12];std::snprintf(key,sizeof(key),"s%02u%c",slot,bank?'b':'a');
        error_=nvs_set_blob(handle_,key,data,length);if(error_==ESP_OK)error_=nvs_commit(handle_);
        return error_==ESP_OK;
    }
    esp_err_t error() const{return error_;}
private:
    nvs_handle_t handle_=0;esp_err_t error_=ESP_OK;
};
}
