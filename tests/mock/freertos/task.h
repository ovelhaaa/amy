#pragma once
#include "FreeRTOS.h"

static inline TickType_t xTaskGetTickCount(void) { return 0; }
static inline void vTaskDelay(TickType_t ticks) {}
static inline void vTaskDelayUntil(TickType_t* prev, TickType_t inc) {}
static inline void vTaskDelete(TaskHandle_t h) {}
