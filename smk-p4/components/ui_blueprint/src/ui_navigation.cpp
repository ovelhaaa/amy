#include "ui_navigation.h"
#include "ui_screens.h"

namespace smk::p4::ui {

const char* areaLabel(UiArea area) {
    switch (area) {
        case UiArea::Tocar:   return "TOCAR";
        case UiArea::Timbres: return "TIMBRES";
        case UiArea::Ritmo:   return "RITMO";
        case UiArea::Fx:      return "FX";
        case UiArea::Mais:    return "MAIS";
        default:              return "---";
    }
}

UiArea screenArea(ScreenId id) {
    switch (id) {
        case ScreenId::Performance:
        case ScreenId::Scenes:
            return UiArea::Tocar;
        case ScreenId::PatchBrowser:
        case ScreenId::SoundEditor:
        case ScreenId::Layers:
        case ScreenId::Mixer:
        case ScreenId::Modulation:
            return UiArea::Timbres;
        case ScreenId::Sequencer:
        case ScreenId::Arpeggiator:
        case ScreenId::Drums:
        case ScreenId::Patterns:
            return UiArea::Ritmo;
        case ScreenId::Effects:
            return UiArea::Fx;
        case ScreenId::MidiSettings:
        case ScreenId::GlobalSettings:
        case ScreenId::Diagnostics:
        case ScreenId::Harmony:
        default:
            return UiArea::Mais;
    }
}

ScreenId defaultScreenForArea(UiArea area) {
    switch (area) {
        case UiArea::Tocar:   return ScreenId::Performance;
        case UiArea::Timbres: return ScreenId::PatchBrowser;
        case UiArea::Ritmo:   return ScreenId::Sequencer;
        case UiArea::Fx:      return ScreenId::Effects;
        case UiArea::Mais:
        default:              return ScreenId::MidiSettings;
    }
}

const char* screenTitle(ScreenId id) {
    switch (id) {
        case ScreenId::Performance:    return "01 PERFORMANCE STAGE";
        case ScreenId::PatchBrowser:   return "02 PATCH BROWSER";
        case ScreenId::SoundEditor:    return "03 SOUND EDITOR";
        case ScreenId::Layers:         return "04 LAYERS & SPLIT";
        case ScreenId::Mixer:          return "05 PERFORMANCE MIXER";
        case ScreenId::Modulation:     return "06 MODULATION MATRIX";
        case ScreenId::Effects:        return "07 MASTER EFFECTS";
        case ScreenId::Arpeggiator:    return "08 ARPEGGIATOR";
        case ScreenId::Sequencer:      return "SEQUENCER";
        case ScreenId::Drums:          return "10 DRUM MACHINE";
        case ScreenId::Harmony:        return "11 HARMONY ASSIST";
        case ScreenId::MidiSettings:   return "12 MIDI & MAPPING";
        case ScreenId::GlobalSettings: return "13 GLOBAL SETTINGS";
        case ScreenId::Diagnostics:    return "14 SYSTEM DIAGNOSTICS";
        case ScreenId::Patterns:       return "PATTERNS";
        case ScreenId::Scenes:         return "SCENES";
        default:                       return "UNKNOWN SCREEN";
    }
}

const char* screenShortName(ScreenId id) {
    switch (id) {
        case ScreenId::Performance:    return "PERF";
        case ScreenId::PatchBrowser:   return "PATCHES";
        case ScreenId::SoundEditor:    return "SOUND";
        case ScreenId::Layers:         return "LAYERS";
        case ScreenId::Mixer:          return "MIXER";
        case ScreenId::Modulation:     return "MOD";
        case ScreenId::Effects:        return "FX";
        case ScreenId::Arpeggiator:    return "ARP";
        case ScreenId::Sequencer:      return "SEQ";
        case ScreenId::Drums:          return "DRUMS";
        case ScreenId::Harmony:        return "HARMONY";
        case ScreenId::MidiSettings:   return "MIDI";
        case ScreenId::GlobalSettings: return "SETTINGS";
        case ScreenId::Diagnostics:    return "DIAG";
        case ScreenId::Patterns:       return "PATTERNS";
        case ScreenId::Scenes:         return "SCENES";
        default:                       return "---";
    }
}

UiMode screenMode(ScreenId id) {
    return (id == ScreenId::Performance) ? UiMode::PerformanceMode : UiMode::EditMode;
}

PresentationLevel screenPresentationLevel(ScreenId id) {
    return (id == ScreenId::Performance) ? PresentationLevel::StageMinimal : PresentationLevel::BlueprintVerbose;
}

ScreenView* createScreenById(ScreenId id) {
    switch (id) {
        case ScreenId::Performance:    return createPerformanceScreen();
        case ScreenId::PatchBrowser:   return createPatchBrowserScreen();
        case ScreenId::SoundEditor:    return createSoundEditorScreen();
        case ScreenId::Layers:         return createLayersScreen();
        case ScreenId::Mixer:          return createMixerScreen();
        case ScreenId::Modulation:     return createModulationScreen();
        case ScreenId::Effects:        return createEffectsScreen();
        case ScreenId::Arpeggiator:    return createArpeggiatorScreen();
        case ScreenId::Sequencer:      return createSequencerScreen();
        case ScreenId::Drums:          return createDrumsScreen();
        case ScreenId::Patterns:       return createPatternsScreen();
        case ScreenId::Harmony:        return createHarmonyScreen();
        case ScreenId::MidiSettings:   return createMidiScreen();
        case ScreenId::GlobalSettings: return createSettingsScreen();
        case ScreenId::Diagnostics:    return createDiagnosticsScreen();
        case ScreenId::Scenes:         return createScenesScreen();
        default:                       return createPerformanceScreen();
    }
}

bool UiNavigationManager::begin(lv_obj_t* content_container, IUiBackend& backend) {
    shutdown();
    if (!content_container) return false;
    content_root_ = content_container;
    backend_ = &backend;
    history_depth_ = 0;
    transitions_ = 0;
    pending_nav_ = false;
    pending_home_ = false;
    for (size_t i = 0; i < kScreenCount; ++i) {
        sub_tabs_[i] = 0;
    }
    for (size_t a = 0; a < kAreaCount; ++a) {
        area_screens_[a] = defaultScreenForArea(static_cast<UiArea>(a));
    }
    current_ = ScreenId::Performance;
    mounted_ = mountScreen(ScreenId::Performance);
    return mounted_;
}

void UiNavigationManager::shutdown() {
    unmountCurrent();
    content_root_ = nullptr;
    backend_ = nullptr;
    mounted_ = false;
    history_depth_ = 0;
    pending_nav_ = false;
    pending_home_ = false;
}

bool UiNavigationManager::navigateTo(ScreenId target, uint8_t sub_tab) {
    if (!content_root_ || !backend_) return false;
    if (static_cast<size_t>(target) >= kScreenCount) return false;

    pending_nav_ = false;
    pending_home_ = false;

    if (sub_tab != 0xFF) {
        sub_tabs_[static_cast<size_t>(target)] = sub_tab;
    }

    if (mounted_ && current_ == target) {
        // Rebuild/refresh current screen if sub_tab changed
        unmountCurrent();
        if (on_navigate_) on_navigate_(on_navigate_user_, current_);
        mounted_ = mountScreen(target);
        return mounted_;
    }

    if (mounted_) {
        if (history_depth_ < kHistoryCapacity) {
            history_[history_depth_++] = current_;
        } else {
            for (size_t i = 1; i < kHistoryCapacity; ++i) {
                history_[i - 1] = history_[i];
            }
            history_[kHistoryCapacity - 1] = current_;
        }
    }

    unmountCurrent();
    current_ = target;
    area_screens_[static_cast<size_t>(screenArea(target))] = target;
    ++transitions_;
    backend_->model().diagnostics().screen_switches = transitions_;
    if (on_navigate_) {
        on_navigate_(on_navigate_user_, current_);
    }
    mounted_ = mountScreen(target);
    return mounted_;
}

bool UiNavigationManager::navigateToArea(UiArea area) {
    if (static_cast<size_t>(area) >= kAreaCount) return false;
    const ScreenId target = area_screens_[static_cast<size_t>(area)];
    return navigateTo(target);
}

ScreenId UiNavigationManager::areaScreen(UiArea area) const {
    const size_t idx = static_cast<size_t>(area);
    return (idx < kAreaCount) ? area_screens_[idx] : ScreenId::Performance;
}

bool UiNavigationManager::back() {
    if (!content_root_ || !backend_) return false;
    pending_nav_ = false;
    pending_home_ = false;
    if (history_depth_ == 0) {
        return returnToPerformance();
    }
    const ScreenId prev = history_[--history_depth_];
    unmountCurrent();
    current_ = prev;
    area_screens_[static_cast<size_t>(screenArea(prev))] = prev;
    ++transitions_;
    backend_->model().diagnostics().screen_switches = transitions_;
    if (on_navigate_) {
        on_navigate_(on_navigate_user_, current_);
    }
    mounted_ = mountScreen(prev);
    return mounted_;
}

bool UiNavigationManager::returnToPerformance() {
    pending_nav_ = false;
    pending_home_ = false;
    history_depth_ = 0;
    area_screens_[static_cast<size_t>(UiArea::Tocar)] = ScreenId::Performance;
    if (mounted_ && current_ == ScreenId::Performance) {
        refreshActive();
        if (on_navigate_) on_navigate_(on_navigate_user_, current_);
        return true;
    }
    unmountCurrent();
    current_ = ScreenId::Performance;
    ++transitions_;
    if (backend_) {
        backend_->model().diagnostics().screen_switches = transitions_;
    }
    if (on_navigate_) {
        on_navigate_(on_navigate_user_, current_);
    }
    mounted_ = mountScreen(ScreenId::Performance);
    return mounted_;
}

void UiNavigationManager::requestNavigateTo(ScreenId target, uint8_t sub_tab) {
    pending_nav_ = true;
    pending_home_ = false;
    pending_target_ = target;
    pending_sub_tab_ = sub_tab;
}

void UiNavigationManager::requestNavigateToArea(UiArea area) {
    if (static_cast<size_t>(area) >= kAreaCount) return;
    requestNavigateTo(area_screens_[static_cast<size_t>(area)]);
}

void UiNavigationManager::requestReturnToPerformance() {
    pending_nav_ = true;
    pending_home_ = true;
    pending_target_ = ScreenId::Performance;
    pending_sub_tab_ = 0xFF;
}

bool UiNavigationManager::processPendingNavigation() {
    if (!pending_nav_) return false;
    const bool home = pending_home_;
    const ScreenId target = pending_target_;
    const uint8_t tab = pending_sub_tab_;
    pending_nav_ = false;
    pending_home_ = false;
    if (home) {
        return returnToPerformance();
    }
    return navigateTo(target, tab);
}

uint8_t UiNavigationManager::subTab(ScreenId screen) const {
    const size_t idx = static_cast<size_t>(screen);
    return (idx < kScreenCount) ? sub_tabs_[idx] : 0;
}

void UiNavigationManager::setSubTab(ScreenId screen, uint8_t tab) {
    const size_t idx = static_cast<size_t>(screen);
    if (idx < kScreenCount) {
        sub_tabs_[idx] = tab;
    }
}

namespace {
void refreshWidgets(lv_obj_t* obj) {
    lv_obj_send_event(obj,LV_EVENT_REFRESH,nullptr);
    const uint32_t count=lv_obj_get_child_count(obj);
    for(uint32_t i=0;i<count;++i) refreshWidgets(lv_obj_get_child(obj,static_cast<int32_t>(i)));
}
}
void UiNavigationManager::refreshActiveView() {
    if(active_view_ && backend_) active_view_->refresh(*backend_);
}

void UiNavigationManager::refreshActive() {
    if (active_view_ && backend_) {
        active_view_->refresh(*backend_);
        if(content_root_) refreshWidgets(content_root_);
    }
}

bool UiNavigationManager::mountScreen(ScreenId target) {
    if (!content_root_ || !backend_) return false;
    lv_obj_clean(content_root_);
    active_view_ = createScreenById(target);
    if (!active_view_) return false;
    if (!active_view_->build(content_root_, *backend_, *this)) {
        active_view_->onDestroy();
        delete active_view_;
        active_view_ = nullptr;
        lv_obj_clean(content_root_);
        return false;
    }
    active_view_->refresh(*backend_);
    return true;
}

void UiNavigationManager::unmountCurrent() {
    // Note lifecycle contract (0.18-C.1): generic screen unmounting NEVER calls
    // releaseAllUiNotes(). USB MIDI, Sustain, Arp, Sequencer, and non-keyboard notes
    // stay alive across navigation. VirtualKeyboard widgets release only their own
    // touch notes via LV_EVENT_DELETE when torn down.
    if (active_view_) {
        active_view_->onDestroy();
        delete active_view_;
        active_view_ = nullptr;
    }
    if (content_root_) {
        lv_obj_clean(content_root_);
    }
}

} // namespace smk::p4::ui
