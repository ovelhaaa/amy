#include "user_preset_nvs_v2.h"
#include <cassert>
#include <cstring>
#include <cstdio>
using namespace smk::p4;
static int mode=0;static char last_key[12]{};static uint8_t stored[160]{};static size_t stored_size=0;
extern "C" esp_err_t nvs_get_blob(nvs_handle_t,const char* key,void* data,size_t* n){
    std::snprintf(last_key,sizeof(last_key),"%s",key);
    if(mode==1)return ESP_ERR_NVS_NOT_FOUND;
    if(mode==2)return ESP_ERR_NVS_INVALID_LENGTH;
    if(*n<stored_size)return ESP_ERR_NVS_INVALID_LENGTH;
    std::memcpy(data,stored,stored_size);*n=stored_size;return ESP_OK;
}
int main(){
    NvsVersionedPresets store(1);uint8_t data[160]{};unsigned n=sizeof(data);
    mode=1;assert(store.readVersioned(1,4,0,data,n)==edit::BankRead::Missing);assert(!std::strcmp(last_key,"s04a"));
    n=sizeof(data);assert(store.readVersioned(2,15,1,data,n)==edit::BankRead::Missing);assert(!std::strcmp(last_key,"v15b"));
    mode=2;n=sizeof(data);assert(store.readVersioned(2,0,0,data,n)==edit::BankRead::Error);
    mode=0;stored_size=128;stored[0]=42;n=sizeof(data);assert(store.readVersioned(1,0,0,data,n)==edit::BankRead::Present && n==128 && data[0]==42);
    n=sizeof(data);assert(store.readVersioned(3,0,0,data,n)==edit::BankRead::Error);
    assert(!store.writeV2(0,0,data,160));assert(!store.write(0,0,data,128));
    NvsVersionedPresets closed(0);n=sizeof(data);assert(closed.readVersioned(1,0,0,data,n)==edit::BankRead::Error);
    std::puts("PASS NVS v2 adapter key mapping, absent/error distinction, disabled writes");
}
