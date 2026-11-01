#pragma once
#include <stdint.h>
#include <chrono>
#include <atomic>
#ifdef SMK_TEST_CLOCK
inline std::atomic<int64_t> smk_test_time_us{1000000};
#endif

static inline int64_t esp_timer_get_time(void) {
#ifdef SMK_TEST_CLOCK
    return smk_test_time_us.load();
#else
    using namespace std::chrono;
    static auto start = steady_clock::now();
    return duration_cast<microseconds>(steady_clock::now() - start).count();
#endif
}

#ifdef SMK_TEST_CLOCK
typedef int esp_err_t;
constexpr int ESP_OK = 0;
#endif
constexpr int ESP_TIMER_TASK = 0;
static inline const char* esp_err_to_name(int) { return "mock"; }
typedef void* esp_timer_handle_t;

struct esp_timer_create_args_t {
    void (*callback)(void* arg);
    void* arg;
    int dispatch_method = 0;
    const char* name;
    bool skip_unhandled_events = false;
};

static inline int esp_timer_create(const esp_timer_create_args_t* create_args, esp_timer_handle_t* out_handle) {
    if (out_handle) *out_handle = (esp_timer_handle_t)1;
    return 0;
}
static inline int esp_timer_start_periodic(esp_timer_handle_t timer, uint64_t period_us) { return 0; }
static inline int esp_timer_stop(esp_timer_handle_t timer) { return 0; }
static inline int esp_timer_delete(esp_timer_handle_t timer) { return 0; }
