#pragma once
#include <stddef.h>
#include <stdint.h>
typedef int esp_err_t;
typedef uint32_t nvs_handle_t;
#define ESP_OK 0
#define ESP_ERR_NVS_NOT_FOUND 1
#define ESP_ERR_NVS_INVALID_LENGTH 2
#define ESP_ERR_NVS_NOT_ENOUGH_SPACE 3
#define ESP_FAIL 4
#define NVS_READWRITE 1
struct nvs_stats_t {size_t free_entries;};
extern "C" {
esp_err_t nvs_get_blob(nvs_handle_t,const char*,void*,size_t*);
esp_err_t nvs_set_blob(nvs_handle_t,const char*,const void*,size_t);
esp_err_t nvs_commit(nvs_handle_t);
esp_err_t nvs_open(const char*,int,nvs_handle_t*);
void nvs_close(nvs_handle_t);
esp_err_t nvs_get_stats(const char*,nvs_stats_t*);
esp_err_t nvs_get_used_entry_count(nvs_handle_t,size_t*);
}
