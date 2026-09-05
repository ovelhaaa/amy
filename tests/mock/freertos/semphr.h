#pragma once
#include "FreeRTOS.h"
#include <chrono>
#include <condition_variable>
#include <mutex>

struct MockSemaphore {
    std::mutex mutex;
    std::condition_variable changed;
    bool available = false;
};
inline SemaphoreHandle_t xSemaphoreCreateBinary() { return new MockSemaphore; }
inline BaseType_t xSemaphoreGive(SemaphoreHandle_t handle) {
    auto* sem = static_cast<MockSemaphore*>(handle);
    std::lock_guard<std::mutex> lock(sem->mutex);
    sem->available = true;
    sem->changed.notify_one();
    return pdTRUE;
}
inline BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t handle, BaseType_t* woken) {
    if (woken) *woken = pdTRUE;
    return xSemaphoreGive(handle);
}
inline BaseType_t xSemaphoreTake(SemaphoreHandle_t handle, TickType_t timeout) {
    auto* sem = static_cast<MockSemaphore*>(handle);
    std::unique_lock<std::mutex> lock(sem->mutex);
    if (!sem->changed.wait_for(lock, std::chrono::milliseconds(timeout), [&] { return sem->available; })) return pdFALSE;
    sem->available = false;
    return pdTRUE;
}
inline void vSemaphoreDelete(SemaphoreHandle_t handle) { delete static_cast<MockSemaphore*>(handle); }
