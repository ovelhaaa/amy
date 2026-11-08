#pragma once
#include "user_presets_v2.h"
#include "nvs.h"
#include <cstdio>
namespace smk::p4 {
// The caller owns the open handle and lifetime. This adapter is not wired into
// the production store until capacity and power-loss recovery are qualified.
class NvsVersionedPresets final : public edit::VersionedPresetStore {
public:
    explicit NvsVersionedPresets(nvs_handle_t handle):handle_(handle) {}
    bool read(unsigned slot,unsigned bank,uint8_t* data,unsigned& length) override {
        return readVersioned(1,slot,bank,data,length)==edit::BankRead::Present;
    }
    bool write(unsigned,unsigned,const uint8_t*,unsigned) override {return false;}
    edit::BankRead readVersioned(unsigned version,unsigned slot,unsigned bank,uint8_t* data,unsigned& length) override {
        if(!handle_ || slot>=edit::kSlots || bank>1 || (version!=1 && version!=2))return edit::BankRead::Error;
        char key[12];makeKey(version,slot,bank,key);
        size_t n=length;error_=nvs_get_blob(handle_,key,data,&n);
        if(error_==ESP_ERR_NVS_NOT_FOUND)return edit::BankRead::Missing;
        if(error_!=ESP_OK)return edit::BankRead::Error;
        length=unsigned(n);return edit::BankRead::Present;
    }
    bool writeV2(unsigned slot,unsigned bank,const uint8_t* data,unsigned length) override {
        // No production admission policy exists yet: never write NVS here.
        (void)slot;(void)bank;(void)data;(void)length;return false;
    }
    esp_err_t error() const{return error_;}
private:
    static void makeKey(unsigned version,unsigned slot,unsigned bank,char* key){
        std::snprintf(key,12,version==1?"s%02u%c":"v%02u%c",slot,bank?'b':'a');
    }
    nvs_handle_t handle_=0;esp_err_t error_=ESP_OK;
};
}
