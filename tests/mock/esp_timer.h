#pragma once
#include <stdint.h>
#include <chrono>

static inline int64_t esp_timer_get_time(void) {
    using namespace std::chrono;
    static auto start = steady_clock::now();
    return duration_cast<microseconds>(steady_clock::now() - start).count();
}

typedef void* esp_timer_handle_t;

struct esp_timer_create_args_t {
    void (*callback)(void* arg);
    void* arg;
    const char* name;
};

static inline int esp_timer_create(const esp_timer_create_args_t* create_args, esp_timer_handle_t* out_handle) {
    if (out_handle) *out_handle = (esp_timer_handle_t)1;
    return 0;
}
static inline int esp_timer_start_periodic(esp_timer_handle_t timer, uint64_t period_us) { return 0; }
static inline int esp_timer_stop(esp_timer_handle_t timer) { return 0; }
static inline int esp_timer_delete(esp_timer_handle_t timer) { return 0; }
