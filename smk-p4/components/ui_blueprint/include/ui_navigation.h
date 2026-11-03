#pragma once
#include "lvgl.h"
#include "ui_model.h"
#include <cstddef>
#include <cstdint>

namespace smk::p4::ui {

enum class ScreenId : uint8_t {
    Performance   = 0,  // TELA 01 — PERFORMANCE
    PatchBrowser  = 1,  // TELA 02 — PATCH BROWSER
    SoundEditor   = 2,  // TELA 03 — SOUND EDITOR (OSC, FM, FILTER, AMP, ENV, LFO, MOD)
    Layers        = 3,  // TELA 04 — LAYERS
    Mixer         = 4,  // TELA 05 — MIXER
    Modulation    = 5,  // TELA 06 — MODULATION
    Effects       = 6,  // TELA 07 — EFFECTS
    Arpeggiator   = 7,  // TELA 08 — ARPEGGIATOR
    Sequencer     = 8,  // TELA 09 — SEQUENCER
    Drums         = 9,  // TELA 10 — DRUMS
    Harmony       = 10, // TELA 11 — HARMONY / SCALE ASSIST
    MidiSettings  = 11, // TELA 12 — MIDI SETTINGS
    GlobalSettings= 12, // TELA 13 — GLOBAL SETTINGS
    Diagnostics   = 13, // TELA 14 — DIAGNOSTICS
    Count         = 14
};

enum class UiMode : uint8_t {
    PerformanceMode = 0,
    EditMode        = 1
};

const char* screenTitle(ScreenId id);
const char* screenShortName(ScreenId id);
UiMode screenMode(ScreenId id);

class UiDemoBackend;
class UiNavigationManager;

// Abstract base for on-demand LVGL screen controllers.
// Only one ScreenView instance has live LVGL widgets at any given moment.
class ScreenView {
public:
    virtual ~ScreenView() = default;
    virtual ScreenId id() const = 0;
    virtual bool build(lv_obj_t* parent, UiDemoBackend& backend, UiNavigationManager& nav) = 0;
    virtual void refresh(UiDemoBackend& backend) = 0;
    virtual void onDestroy() {}
};

class UiNavigationManager {
public:
    static constexpr size_t kHistoryCapacity = 16;
    static constexpr size_t kScreenCount     = static_cast<size_t>(ScreenId::Count);

    UiNavigationManager() = default;
    ~UiNavigationManager() { shutdown(); }

    bool begin(lv_obj_t* content_container, UiDemoBackend& backend);
    void shutdown();

    bool navigateTo(ScreenId target, uint8_t sub_tab = 0xFF);
    bool back();
    bool returnToPerformance();

    ScreenId currentScreen() const { return current_; }
    UiMode currentMode() const { return screenMode(current_); }
    uint8_t subTab(ScreenId screen) const;
    void setSubTab(ScreenId screen, uint8_t tab);

    size_t historyDepth() const { return history_depth_; }
    uint32_t transitionCount() const { return transitions_; }
    ScreenView* activeView() const { return active_view_; }

    void refreshActive();

    // Nav bar bank toggle (0 = Primary 6: PERF/SOUND/LAYERS/FX/SEQ/HARMONY,
    // 1 = Secondary: PERF/PATCHES/MIXER/MOD/ARP/DRUMS,
    // 2 = System: PERF/MIDI/SETTINGS/DIAG)
    uint8_t navBank() const { return nav_bank_; }
    void setNavBank(uint8_t bank) { nav_bank_ = bank % 3; }
    void cycleNavBank() { nav_bank_ = (nav_bank_ + 1) % 3; }

    using NavigationCallback = void (*)(void* user_data, ScreenId new_screen);
    void setOnNavigate(NavigationCallback cb, void* user_data) {
        on_navigate_ = cb;
        on_navigate_user_ = user_data;
    }

private:
    bool mountScreen(ScreenId target);
    void unmountCurrent();

    lv_obj_t*          content_root_     = nullptr;
    UiDemoBackend*     backend_          = nullptr;
    ScreenView*        active_view_      = nullptr;
    ScreenId           current_          = ScreenId::Performance;
    ScreenId           history_[kHistoryCapacity]{};
    size_t             history_depth_    = 0;
    uint8_t            sub_tabs_[kScreenCount]{};
    uint8_t            nav_bank_         = 0;
    uint32_t           transitions_      = 0;
    bool               mounted_          = false;
    NavigationCallback on_navigate_      = nullptr;
    void*              on_navigate_user_ = nullptr;
};

} // namespace smk::p4::ui
