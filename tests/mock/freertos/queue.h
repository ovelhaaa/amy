#pragma once
#include "FreeRTOS.h"
#include <deque>
#include <vector>
#include <cstring>
#include <cstdint>

struct MockQueue {
    uint32_t max_len = 0;
    uint32_t item_size = 0;
    std::deque<std::vector<uint8_t>> items;
};

static inline QueueHandle_t xQueueCreate(uint32_t len, uint32_t item_size) {
    auto* q = new MockQueue();
    q->max_len = len;
    q->item_size = item_size;
    return (QueueHandle_t)q;
}

static inline int xQueueSend(QueueHandle_t q, const void* item, TickType_t wait) {
    if (!q || !item) return pdFALSE;
    auto* mq = (MockQueue*)q;
    if (mq->items.size() >= mq->max_len) return errQUEUE_FULL;
    std::vector<uint8_t> data(mq->item_size);
    memcpy(data.data(), item, mq->item_size);
    mq->items.push_back(std::move(data));
    return pdTRUE;
}

static inline int xQueueSendToBack(QueueHandle_t q, const void* item, TickType_t wait) {
    return xQueueSend(q, item, wait);
}

static inline int xQueueSendToFront(QueueHandle_t q, const void* item, TickType_t wait) {
    if (!q || !item) return pdFALSE;
    auto* mq = (MockQueue*)q;
    if (mq->items.size() >= mq->max_len) return errQUEUE_FULL;
    std::vector<uint8_t> data(mq->item_size);
    memcpy(data.data(), item, mq->item_size);
    mq->items.push_front(std::move(data));
    return pdTRUE;
}

static inline int xQueueSendFromISR(QueueHandle_t q, const void* item, BaseType_t* woken) {
    return xQueueSend(q, item, 0);
}

static inline int xQueueSendToFrontFromISR(QueueHandle_t q, const void* item, BaseType_t* woken) {
    return xQueueSendToFront(q, item, 0);
}

static inline size_t uxQueueMessagesWaiting(QueueHandle_t q) {
    if (!q) return 0;
    return ((MockQueue*)q)->items.size();
}

static inline int xQueueReceive(QueueHandle_t q, void* buffer, TickType_t wait) {
    if (!q || !buffer) return pdFALSE;
    auto* mq = (MockQueue*)q;
    if (mq->items.empty()) return pdFALSE;
    memcpy(buffer, mq->items.front().data(), mq->item_size);
    mq->items.pop_front();
    return pdTRUE;
}

static inline int xQueueReceiveFromISR(QueueHandle_t q, void* buffer, BaseType_t* woken) {
    return xQueueReceive(q, buffer, 0);
}

static inline void vQueueDelete(QueueHandle_t q) {
    if (q) {
        delete (MockQueue*)q;
    }
}
