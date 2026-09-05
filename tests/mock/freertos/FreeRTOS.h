#pragma once
#include <stdint.h>
#include <stddef.h>

#define pdMS_TO_TICKS(ms) ((uint32_t)(ms))
#define pdTRUE  1
#define pdPASS  1
#define pdFALSE 0
#define errQUEUE_FULL 0
#define portMAX_DELAY 0xFFFFFFFF

typedef uint32_t TickType_t;
typedef int32_t BaseType_t;
typedef uint32_t UBaseType_t;
typedef void* QueueHandle_t;
typedef void* TaskHandle_t;
typedef void* SemaphoreHandle_t;

#ifdef __cplusplus
#include <mutex>
using portMUX_TYPE = std::recursive_mutex;
#define portMUX_INITIALIZER_UNLOCKED {}
#define portENTER_CRITICAL(mux) (mux)->lock()
#define portEXIT_CRITICAL(mux) (mux)->unlock()
#define portENTER_CRITICAL_ISR(mux) portENTER_CRITICAL(mux)
#define portEXIT_CRITICAL_ISR(mux) portEXIT_CRITICAL(mux)
#endif
