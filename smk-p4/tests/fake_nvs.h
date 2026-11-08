#pragma once
#include "nvs.h"
#include <map>
#include <vector>
#include <string>
#include <cstring>
namespace fake_nvs {
inline std::map<std::pair<unsigned,std::string>,std::vector<uint8_t>> blobs;
inline size_t free_entries=700;
inline bool read_error=false,torn=false,commit_error=false,stats_error=false;
inline unsigned writes=0;
}
extern "C" {
esp_err_t nvs_flash_init(){return ESP_OK;}
esp_err_t nvs_open(const char* name,int,nvs_handle_t* h){*h=std::strcmp(name,"sound011")==0?1:2;return ESP_OK;}
void nvs_close(nvs_handle_t){}
esp_err_t nvs_get_blob(nvs_handle_t h,const char* key,void* data,size_t* n){
    if(fake_nvs::read_error)return ESP_FAIL;
    const auto it=fake_nvs::blobs.find({h,key});if(it==fake_nvs::blobs.end())return ESP_ERR_NVS_NOT_FOUND;
    if(!data){*n=it->second.size();return ESP_OK;}
    if(*n<it->second.size()){*n=it->second.size();return ESP_ERR_NVS_INVALID_LENGTH;}
    *n=it->second.size();std::memcpy(data,it->second.data(),*n);return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h,const char* key,const void* data,size_t n){
    ++fake_nvs::writes;
    auto& blob=fake_nvs::blobs[{h,key}];if(blob.empty())fake_nvs::free_entries-=8;
    const auto* p=static_cast<const uint8_t*>(data);blob.assign(p,p+(fake_nvs::torn?n/2:n));
    return fake_nvs::torn?ESP_FAIL:ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t){return fake_nvs::commit_error?ESP_FAIL:ESP_OK;}
esp_err_t nvs_get_stats(const char*,nvs_stats_t* s){if(fake_nvs::stats_error)return ESP_FAIL;s->free_entries=fake_nvs::free_entries;return ESP_OK;}
esp_err_t nvs_get_used_entry_count(nvs_handle_t h,size_t* n){*n=0;for(const auto& pair:fake_nvs::blobs)if(pair.first.first==h)*n+=8;return ESP_OK;}
}
