#pragma once
#include "FreeRTOS.h"

static inline QueueHandle_t xQueueCreate(uint32_t len, uint32_t item_size) { return (QueueHandle_t)1; }
static inline int xQueueSend(QueueHandle_t q, const void* item, TickType_t wait) { return pdTRUE; }
static inline int xQueueSendToBack(QueueHandle_t q, const void* item, TickType_t wait) { return pdTRUE; }
static inline int xQueueSendToFront(QueueHandle_t q, const void* item, TickType_t wait) { return pdTRUE; }
static inline int xQueueSendFromISR(QueueHandle_t q, const void* item, BaseType_t* woken) { return pdTRUE; }
static inline size_t uxQueueMessagesWaiting(QueueHandle_t q) { return 0; }
static inline int xQueueReceive(QueueHandle_t q, void* buffer, TickType_t wait) { return pdTRUE; }
static inline void vQueueDelete(QueueHandle_t q) {}
