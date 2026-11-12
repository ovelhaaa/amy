#pragma once

#include "controller_profile.h"
#include "nvs.h"

namespace smk::p4::performance {

class NvsControllerProfile final : public ControllerProfileStore {
public:
    bool load(ControllerProfile& profile) override {
        nvs_handle_t handle;
        if(nvs_open("perf014midi",NVS_READONLY,&handle)!=ESP_OK) return false;
        uint8_t bytes[kProfileBytes]{};
        size_t length=sizeof(bytes);
        const auto error=nvs_get_blob(handle,"profile",bytes,&length);
        nvs_close(handle);
        return error==ESP_OK && decodeProfile(bytes,unsigned(length),profile);
    }

    bool save(const ControllerProfile& profile) override {
        uint8_t bytes[kProfileBytes]{};
        if(!encodeProfile(profile,bytes)) return false;
        nvs_handle_t handle;
        if(nvs_open("perf014midi",NVS_READWRITE,&handle)!=ESP_OK) return false;
        auto error=nvs_set_blob(handle,"profile",bytes,sizeof(bytes));
        if(error==ESP_OK) error=nvs_commit(handle);
        nvs_close(handle);
        if(error!=ESP_OK) return false;
        ControllerProfile check;
        return load(check) && check.bank==profile.bank && check.knobs==profile.knobs && check.destinations==profile.destinations;
    }
};

}
