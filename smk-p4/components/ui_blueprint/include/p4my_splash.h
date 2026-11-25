#pragma once
#include "lvgl.h"
#include <cstdint>

namespace smk::p4::ui {
// All methods, including destruction, belong to the existing LVGL owner.
// No task, timer, audio dependency or per-frame buffer allocation.
class P4mySplash {
public:
    enum class State : uint8_t {
        Inactive = 0,
        Animating,
        FinalHold,
        WaitingForEngine,
        Exiting,
        Closed
    };

    enum class DismissReason : uint8_t {
        None = 0,
        AnimationComplete,
        Touch,
        MidiNoteOn,
        ExplicitCommand,
        EngineError,
        ParentDeleted
    };

    struct Config {
        uint32_t duration_ms     = 2700;
        uint32_t final_hold_ms   = 250;
        uint32_t exit_ms         = 180;
        uint32_t wait_timeout_ms = 8000;
        uint8_t  fps             = 30;
    };

    struct Metrics {
        uint32_t frames_rendered       = 0;
        uint32_t wave_updates          = 0;
        uint32_t render_total_us       = 0;
        uint32_t render_max_us         = 0;
        uint32_t engine_ready_ms       = 0;
        uint32_t effective_duration_ms = 0;
        bool     engine_ready_recorded = false;
        DismissReason dismiss_reason   = DismissReason::None;
    };

    P4mySplash() = default;
    ~P4mySplash() { close(); }
    P4mySplash(const P4mySplash&) = delete;
    P4mySplash& operator=(const P4mySplash&) = delete;

    bool begin(lv_obj_t* parent, uint32_t now_ms, Config config);
    bool begin(lv_obj_t* parent, uint32_t now_ms) { return begin(parent, now_ms, Config{}); }
    void service(uint32_t now_ms);
    void setEngineReady(bool ready, uint32_t now_ms = 0);
    void setEngineError(bool error, uint32_t now_ms);
    void requestDismiss(DismissReason reason, uint32_t now_ms);
    void finish(uint32_t now_ms); // Explicit dismiss; idempotent, bounded fade.
    void seek(uint32_t elapsed_ms); // Manual preview, independent of wall clock.
    void close();

    bool active() const { return root_ != nullptr; }
    State state() const { return state_; }
    DismissReason dismissReason() const { return metrics_.dismiss_reason; }
    const Metrics& metrics() const { return metrics_; }
    lv_obj_t* object() const { return root_; }

private:
    static void deleted(lv_event_t* event);
    static void touched(lv_event_t* event);
    void render(uint32_t elapsed_ms);
    void beginExit(DismissReason reason, uint32_t now_ms);

    lv_obj_t* root_      = nullptr;
    lv_obj_t* technical_ = nullptr;
    lv_obj_t* waves_[5]{};
    lv_obj_t* logo_      = nullptr;
    lv_obj_t* subtitle_  = nullptr;
    lv_obj_t* credit_    = nullptr;
    static constexpr unsigned kPoints = 161;
    lv_point_precise_t points_[3][kPoints]{};
    Config config_{};
    Metrics metrics_{};
    State state_ = State::Inactive;
    uint32_t start_ms_ = 0, last_ms_ = 0, hold_start_ms_ = 0, wait_start_ms_ = 0, exit_start_ms_ = 0;
    uint32_t current_now_ms_ = 0;
    bool engine_ready_ = false;
    bool engine_error_ = false;
    bool final_rendered_ = false;
};
} // namespace smk::p4::ui
