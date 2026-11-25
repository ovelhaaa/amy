#pragma once
#include "lvgl.h"
#include "ui_backend.h"
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
    Scenes        = 14,
    Patterns      = 15,
    Count         = 16
};

enum class UiArea : uint8_t {
    Tocar   = 0,
    Timbres = 1,
    Ritmo   = 2,
    Fx      = 3,
    Mais    = 4,
    Count   = 5
};

enum class UiMode : uint8_t {
    PerformanceMode = 0,
    EditMode        = 1
};

const char* areaLabel(UiArea area);
UiArea screenArea(ScreenId id);
ScreenId defaultScreenForArea(UiArea area);
const char* screenTitle(ScreenId id);
const char* screenShortName(ScreenId id);
UiMode screenMode(ScreenId id);
PresentationLevel screenPresentationLevel(ScreenId id);

class UiNavigationManager;

// Abstract base for on-demand LVGL screen controllers.
// Only one ScreenView instance has live LVGL widgets at any given moment.
class ScreenView {
public:
    virtual ~ScreenView() = default;
    virtual ScreenId id() const = 0;
    virtual bool build(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav) = 0;
    virtual void refresh(IUiBackend& backend) = 0;
    virtual void onDestroy() {}
    virtual bool hasBlockingConfirmation() const { return false; }
};

class UiNavigationManager {
public:
    static constexpr size_t kHistoryCapacity = 16;
    static constexpr size_t kScreenCount     = static_cast<size_t>(ScreenId::Count);
    static constexpr size_t kAreaCount       = static_cast<size_t>(UiArea::Count);

    UiNavigationManager() = default;
    ~UiNavigationManager() { shutdown(); }

    bool begin(lv_obj_t* content_container, IUiBackend& backend);
    void shutdown();

    // Direct navigation (preserves USB MIDI, Arp, Sequencer, and non-keyboard notes;
    // VirtualKeyboard widgets release their own touch notes on LV_EVENT_DELETE)
    bool navigateTo(ScreenId target, uint8_t sub_tab = 0xFF);
    bool navigateToArea(UiArea area);
    bool back();
    bool returnToPerformance();

    // Deferred navigation: schedules a screen, area, or sub-tab transition to be processed
    // safely outside of an active LVGL widget callback (e.g. inside UiBlueprint::service).
    void requestNavigateTo(ScreenId target, uint8_t sub_tab = 0xFF);
    void requestNavigateToArea(UiArea area);
    void requestReturnToPerformance();
    bool hasPendingNavigation() const { return pending_nav_; }
    bool processPendingNavigation();

    ScreenId currentScreen() const { return current_; }
    UiArea currentArea() const { return screenArea(current_); }
    ScreenId areaScreen(UiArea area) const;
    UiMode currentMode() const { return screenMode(current_); }
    PresentationLevel currentPresentationLevel() const { return screenPresentationLevel(current_); }
    uint8_t subTab(ScreenId screen) const;
    void setSubTab(ScreenId screen, uint8_t tab);

    bool sequencerEdit() const { return sequencer_edit_; }
    void setSequencerEdit(bool edit) { sequencer_edit_=edit; }
    uint8_t sequencerPage() const { return sequencer_page_; }
    void setSequencerPage(uint8_t page) { sequencer_page_ = static_cast<uint8_t>(page & 1u); }
    uint8_t patternSelection() const { return pattern_selection_; }
    void setPatternSelection(uint8_t slot) { if(slot<seq::kPatternSlots) pattern_selection_=slot; }
    seq::SwitchMode patternSwitchMode() const { return pattern_switch_mode_; }
    void setPatternSwitchMode(seq::SwitchMode mode) { pattern_switch_mode_ = mode; }
    size_t historyDepth() const { return history_depth_; }
    uint32_t transitionCount() const { return transitions_; }
    ScreenView* activeView() const { return active_view_; }

    void refreshActive();
    void refreshActiveView();
    lv_obj_t* contentRoot() const { return content_root_; }

    using NavigationCallback = void (*)(void* user_data, ScreenId new_screen);
    void setOnNavigate(NavigationCallback cb, void* user_data) {
        on_navigate_ = cb;
        on_navigate_user_ = user_data;
    }

    using HarmonyOverlayCallback = void (*)(void* user_data);
    void setOnOpenHarmonyOverlay(HarmonyOverlayCallback cb, void* user_data) {
        on_harmony_overlay_ = cb;
        on_harmony_overlay_user_ = user_data;
    }
    bool requestOpenHarmonyOverlay() {
        if (!on_harmony_overlay_) return false;
        on_harmony_overlay_(on_harmony_overlay_user_);
        return true;
    }

private:
    bool mountScreen(ScreenId target);
    void unmountCurrent();

    lv_obj_t*          content_root_     = nullptr;
    IUiBackend*        backend_          = nullptr;
    ScreenView*        active_view_      = nullptr;
    ScreenId           current_          = ScreenId::Performance;
    ScreenId           area_screens_[kAreaCount]{
        ScreenId::Performance,
        ScreenId::PatchBrowser,
        ScreenId::Sequencer,
        ScreenId::Effects,
        ScreenId::MidiSettings
    };
    ScreenId           history_[kHistoryCapacity]{};
    size_t             history_depth_    = 0;
    uint8_t            sub_tabs_[kScreenCount]{};
    uint32_t           transitions_      = 0;
    bool sequencer_edit_=false;
    uint8_t sequencer_page_=0;
    uint8_t pattern_selection_=0;
    seq::SwitchMode pattern_switch_mode_=seq::SwitchMode::Immediate;
    bool               mounted_          = false;
    bool               pending_nav_      = false;
    bool               pending_home_     = false;
    ScreenId           pending_target_   = ScreenId::Performance;
    uint8_t            pending_sub_tab_  = 0xFF;
    NavigationCallback     on_navigate_             = nullptr;
    void*                  on_navigate_user_        = nullptr;
    HarmonyOverlayCallback on_harmony_overlay_      = nullptr;
    void*                  on_harmony_overlay_user_ = nullptr;
};

} // namespace smk::p4::ui
