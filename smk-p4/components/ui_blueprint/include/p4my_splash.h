#pragma once
#include "lvgl.h"
#include <cstdint>

namespace smk::p4::ui {
// All methods, including destruction, belong to the existing LVGL owner.
// No task, timer, audio dependency or per-frame buffer allocation.
class P4mySplash {
public:
    struct Config {
        uint32_t duration_ms = 2700;
        uint32_t exit_ms = 180;
        uint8_t fps = 30;
    };
    P4mySplash() = default;
    ~P4mySplash() { close(); }
    P4mySplash(const P4mySplash&) = delete;
    P4mySplash& operator=(const P4mySplash&) = delete;
    bool begin(lv_obj_t* parent, uint32_t now_ms, Config config);
    bool begin(lv_obj_t* parent, uint32_t now_ms) { return begin(parent, now_ms, Config{}); }
    void service(uint32_t now_ms);
    void finish(uint32_t now_ms); // Real readiness only; idempotent, bounded fade.
    void seek(uint32_t elapsed_ms); // Manual preview, independent of wall clock.
    void close();
    bool active() const { return root_ != nullptr; }
    lv_obj_t* object() const { return root_; }
private:
    static void deleted(lv_event_t* event);
    void render(uint32_t elapsed_ms);
    lv_obj_t* root_ = nullptr;
    lv_obj_t* technical_ = nullptr;
    lv_obj_t* waves_[5]{};
    lv_obj_t* logo_ = nullptr;
    lv_obj_t* subtitle_ = nullptr;
    lv_obj_t* credit_ = nullptr;
    static constexpr unsigned kPoints = 161;
    lv_point_precise_t points_[3][kPoints]{};
    Config config_{};
    uint32_t start_ms_ = 0, last_ms_ = 0, exit_start_ms_ = 0;
    bool exiting_ = false;
};
} // namespace smk::p4::ui
