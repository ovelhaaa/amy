#pragma once
#include <atomic>
#include <cstdint>
namespace mock_time { inline std::atomic<int64_t> us{0}; }
inline int64_t esp_timer_get_time() { return mock_time::us.load(); }
