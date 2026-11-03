#pragma once
#include "../../../../tests/mock/freertos/task.h"
inline BaseType_t xTaskCreate(void (*routine)(void*),const char* name,uint32_t stack,void* arg,UBaseType_t priority,TaskHandle_t* handle) {
    return xTaskCreatePinnedToCore(routine,name,stack,arg,priority,handle,0);
}
inline unsigned uxTaskGetStackHighWaterMark(void*) { return 4096; }
