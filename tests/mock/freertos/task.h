#pragma once
#include "FreeRTOS.h"

static inline TickType_t xTaskGetTickCount(void) { return 0; }
static inline void vTaskDelay(TickType_t ticks) {}
static inline void vTaskDelayUntil(TickType_t* prev, TickType_t inc) {}
static inline void vTaskDelete(TaskHandle_t h) {}

#ifdef __cplusplus
namespace mock_task {
inline BaseType_t create_result = pdPASS;
inline void (*routine)(void*) = nullptr;
inline void* argument = nullptr;
inline void run() { routine(argument); }
}
inline BaseType_t xTaskCreatePinnedToCore(void (*routine)(void*), const char*, uint32_t,
                                        void* argument, UBaseType_t, TaskHandle_t* handle, BaseType_t) {
    if (mock_task::create_result != pdPASS) return mock_task::create_result;
    mock_task::routine = routine;
    mock_task::argument = argument;
    *handle = reinterpret_cast<TaskHandle_t>(1);
    return pdPASS;
}
inline int xPortGetCoreID() { return 1; }
#endif
