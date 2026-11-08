#pragma once
#include "user_presets_v2.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <cstdio>
namespace smk::p4 {
// Control task only. Legacy namespace stays intact for older firmware.
class NvsVersionedPresets final : public edit::VersionedPresetStore {
public:
    ~NvsVersionedPresets() override {close();}
    NvsVersionedPresets()=default;
    NvsVersionedPresets(const NvsVersionedPresets&)=delete;
    NvsVersionedPresets& operator=(const NvsVersionedPresets&)=delete;
    bool begin(){
        close();error_=nvs_flash_init();if(error_!=ESP_OK)return false;
        error_=nvs_open("sound011",NVS_READWRITE,&legacy_);if(error_!=ESP_OK)return false;
        error_=nvs_open("sound011v2",NVS_READWRITE,&modern_);
        if(error_!=ESP_OK){close();return false;}
        return true; // Read/Recall remain usable even if writes lack space.
    }
    bool read(unsigned slot,unsigned bank,uint8_t* data,unsigned& length) override {
        return readVersioned(1,slot,bank,data,length)==edit::BankRead::Present;
    }
    bool write(unsigned,unsigned,const uint8_t*,unsigned) override {return false;}
    edit::BankRead readVersioned(unsigned version,unsigned slot,unsigned bank,uint8_t* data,unsigned& length) override {
        if(!legacy_ || !modern_ || slot>=edit::kSlots || bank>1 || (version!=1 && version!=2))return edit::BankRead::Error;
        char key[12];makeKey(version,slot,bank,key);
        size_t n=length;error_=nvs_get_blob(version==1?legacy_:modern_,key,data,&n);
        if(error_==ESP_ERR_NVS_NOT_FOUND)return edit::BankRead::Missing;
        if(error_!=ESP_OK)return edit::BankRead::Error;
        length=unsigned(n);return edit::BankRead::Present;
    }
    bool writeV2(unsigned slot,unsigned bank,const uint8_t* data,unsigned length) override {
        if(!modern_ || slot>=edit::kSlots || bank>1 || length!=edit::kV2RecordBytes)return false;
        nvs_stats_t stats{};size_t used=0;
        error_=nvs_get_stats(nullptr,&stats);if(error_!=ESP_OK)return false;
        error_=nvs_get_used_entry_count(modern_,&used);if(error_!=ESP_OK)return false;
        // ESP-IDF 5.5.2: 160 bytes/32 + one chunk header + blob index = 7
        // entries normally, conservatively budget 8 (including a split chunk).
        // Reserve a GC page and one full record beyond all 32 resident banks.
        constexpr size_t entries_per_blob=8,gc_entries=126;
        constexpr size_t required=edit::kSlots*2*entries_per_blob+gc_entries+entries_per_blob;
        if(stats.free_entries+used<required || stats.free_entries<gc_entries+entries_per_blob){
            error_=ESP_ERR_NVS_NOT_ENOUGH_SPACE;return false;
        }
        char key[12];makeKey(2,slot,bank,key);
        error_=nvs_set_blob(modern_,key,data,length);
        if(error_==ESP_OK)error_=nvs_commit(modern_);
        return error_==ESP_OK;
    }
    esp_err_t error() const{return error_;}
private:
    void close(){if(legacy_)nvs_close(legacy_);if(modern_)nvs_close(modern_);legacy_=modern_=0;}
    static void makeKey(unsigned version,unsigned slot,unsigned bank,char* key){
        std::snprintf(key,12,version==1?"s%02u%c":"v%02u%c",slot,bank?'b':'a');
    }
    nvs_handle_t legacy_=0,modern_=0;esp_err_t error_=ESP_OK;
};
}
