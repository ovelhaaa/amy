#pragma once
#include "lvgl.h"
#include "ui_backend.h"
#include "ui_navigation.h"
#include "ui_theme.h"
#include <cstddef>
#include <cstdint>

namespace smk::p4::ui {

void servicePatchConfirmation(UiNavigationManager& nav);
void stepPatchWithConfirmation(IUiBackend&, UiNavigationManager&, int direction);
void selectPatchWithConfirmation(IUiBackend&, UiNavigationManager&, uint16_t, uint8_t, bool force_dialog = false);

uint8_t sessionLockMask();
void setSessionLockMask(uint8_t mask);

// Helper to create a clipped label with explicit font and color
lv_obj_t* createLabel(lv_obj_t* parent, const char* text, int32_t x, int32_t y, int32_t w,
                      const lv_font_t* font = &lv_font_montserrat_14,
                      uint32_t color_hex = palette::kTextPrimary);

// Helper to create a touch-friendly industrial button (minimum 36px height recommended)
lv_obj_t* createActionButton(lv_obj_t* parent, const char* text, int32_t x, int32_t y,
                             int32_t w, int32_t h, lv_event_cb_t cb, void* user_data,
                             bool active = false, bool accent = false);

// Helper to attach heap-safe user_data allocated via lv_malloc that auto-frees on LV_EVENT_DELETE
template <typename T>
T* attachLvContext(lv_obj_t* obj, const T& value, lv_event_cb_t handler, lv_event_code_t filter = LV_EVENT_ALL) {
    if (!obj) return nullptr;
    void* raw = lv_malloc(sizeof(T));
    if (!raw) return nullptr;
    T* ctx = static_cast<T*>(raw);
    *ctx = value;
    if (handler) {
        lv_obj_add_event_cb(obj, handler, filter, ctx);
    }
    lv_obj_add_event_cb(
        obj,
        [](lv_event_t* e) {
            if (lv_event_get_code(e) == LV_EVENT_DELETE) {
                void* data = lv_event_get_user_data(e);
                if (data) lv_free(data);
            }
        },
        LV_EVENT_DELETE, ctx);
    return ctx;
}

// ── Top Global Musical Header (800 x 44, Variant A + Overlays) ───────────
class StatusStrip {
public:
    enum class OverlayKind : uint8_t {
        None = 0,
        Tempo = 1,
        Harmony = 2,
        AreaModules = 3
    };

    bool build(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav);
    void refresh(IUiBackend& backend, UiNavigationManager& nav);
    void refreshTempo(IUiBackend& backend);
    void closeOverlays(bool async = false);
    void openTempo(IUiBackend& backend);
    void openHarmony(IUiBackend& backend);
    void openAreaModules(UiArea area, IUiBackend& backend, UiNavigationManager& nav);
    void toggleAreaModules(UiArea area, IUiBackend& backend, UiNavigationManager& nav);
    OverlayKind activeOverlay() const { return active_overlay_; }
    UiArea overlayArea() const { return overlay_area_; }

private:
    void showOverlayBackdrop();
    void toggleTempo(IUiBackend& backend);
    void toggleHarmony(IUiBackend& backend);

    lv_obj_t* parent_root_ = nullptr;
    IUiBackend* backend_   = nullptr;
    UiNavigationManager* nav_ = nullptr;
    lv_obj_t* root_        = nullptr;
    lv_obj_t* demo_badge_  = nullptr;
    lv_obj_t* patch_lbl_   = nullptr;
    lv_obj_t* midi_lbl_    = nullptr;
    lv_obj_t* audio_lbl_   = nullptr;
    lv_obj_t* tempo_btn_   = nullptr;
    lv_obj_t* tempo_label_ = nullptr;
    lv_obj_t* harm_btn_    = nullptr;
    lv_obj_t* harm_lbl_    = nullptr;
    lv_obj_t* q_btn_       = nullptr;
    lv_obj_t* q_lbl_       = nullptr;
    lv_obj_t* ch_btn_      = nullptr;
    lv_obj_t* ch_lbl_      = nullptr;
    lv_obj_t* mods_btn_    = nullptr;
    lv_obj_t* mods_lbl_    = nullptr;
    lv_obj_t* panic_btn_   = nullptr;

    // Overlay container (covers only y = kContentTop .. kContentBottom - 1)
    OverlayKind active_overlay_ = OverlayKind::None;
    UiArea      overlay_area_   = UiArea::Timbres;
    lv_obj_t*   overlay_backdrop_ = nullptr;
    lv_obj_t*   overlay_card_     = nullptr;
    lv_obj_t*   tempo_notice_     = nullptr;
    lv_obj_t*   tempo_slider_     = nullptr;
    lv_obj_t*   tempo_big_lbl_    = nullptr;
    lv_obj_t*   harm_status_lbl_  = nullptr;
    lv_obj_t*   harm_q_btn_       = nullptr;
    lv_obj_t*   harm_ch_btn_      = nullptr;
    lv_obj_t*   harm_touch_btn_   = nullptr;
    lv_obj_t*   harm_preset_btn_  = nullptr;
    lv_obj_t*   harm_row_lbls_[5]{};
    lv_obj_t*   harm_row_prev_[5]{};
    lv_obj_t*   harm_row_next_[5]{};
    uint32_t    harm_pending_token_ = 0;
    bool        harm_awaiting_token_ = false;
    const char* harm_pending_param_ = nullptr;

    char      patch_buf_[48]{};
    char      tempo_buf_[24]{};
    char      harm_buf_[24]{};
    char      q_buf_[16]{};
    char      ch_buf_[16]{};
    char      bpm_big_buf_[16]{};
    char      harm_status_buf_[128]{};
    char      harm_preset_buf_[32]{};
    char      harm_row_bufs_[5][48]{};
};

// ── Reusable Confirmation Modal (keeps top header + PANIC uncovered) ─────
struct ConfirmDialogOptions {
    const char* title = nullptr;
    const char* body = nullptr;
    const char* detail = nullptr;
    const char* cancel_label = "CANCELAR";
    const char* confirm_label = "CONFIRMAR";
    uint32_t border_color = palette::kAccentPrimary;
    bool destructive = false;
    void (*on_confirm)(void* user_data) = nullptr;
    void (*on_cancel)(void* user_data) = nullptr;
    void* user_data = nullptr;
};

class ConfirmDialog {
public:
    bool open(lv_obj_t* parent, const ConfirmDialogOptions& opts);
    void close(bool async = true);
    bool isOpen() const { return backdrop_ != nullptr; }

private:
    lv_obj_t* backdrop_ = nullptr;
};

// ── Bottom Navigation Bar (800 x 48, 5 Fixed Areas) & Internal Sub-Tab Bar
class BottomNavBar {
public:
    using AreaClickCallback = void (*)(void* user_data, UiArea area);

    bool build(lv_obj_t* parent, UiNavigationManager& nav,
               AreaClickCallback on_area_click = nullptr, void* user_data = nullptr);
    void refresh(UiNavigationManager& nav);

private:
    lv_obj_t*         root_ = nullptr;
    lv_obj_t*         buttons_[5]{};
    lv_obj_t*         labels_[5]{};
    AreaClickCallback on_area_click_ = nullptr;
    void*             on_area_user_  = nullptr;
};

lv_obj_t* createSubTabBar(lv_obj_t* parent, const char* const* tab_names, uint8_t tab_count,
                          uint8_t active_tab, ScreenId screen_id, UiNavigationManager& nav,
                          int32_t x = 8, int32_t y = 4, int32_t w = 784, int32_t h = 36);

// ── Parameter Widgets (bound to IUiBackend by canonical parameter ID) ────
void showUserPresetPanel(lv_obj_t* anchor,IUiBackend& backend);
void showFmAlgorithmPicker(lv_obj_t* anchor,IUiBackend& backend);
lv_obj_t* createFmAlgorithmSelector(lv_obj_t* parent, IUiBackend& backend,
                                    int32_t x, int32_t y, int32_t w, int32_t h);
lv_obj_t* createFmTopologyDiagram(lv_obj_t* parent, IUiBackend& backend,
                                  int32_t x, int32_t y, int32_t w, int32_t h);

lv_obj_t* createParameterKnob(lv_obj_t* parent, IUiBackend& backend, const char* param_id,
                              int32_t x, int32_t y, int32_t w = 122, int32_t h = 114,
                              PresentationLevel level = PresentationLevel::BlueprintVerbose,
                              const char* label_override = nullptr);

lv_obj_t* createParameterSlider(lv_obj_t* parent, IUiBackend& backend, const char* param_id,
                                int32_t x, int32_t y, int32_t w = 188, int32_t h = 76,
                                PresentationLevel level = PresentationLevel::BlueprintVerbose,
                                const char* label_override = nullptr);

lv_obj_t* createParameterCard(lv_obj_t* parent, IUiBackend& backend, const char* param_id,
                              int32_t x, int32_t y, int32_t w = 188, int32_t h = 82,
                              PresentationLevel level = PresentationLevel::BlueprintVerbose,
                              const char* label_override = nullptr);

// ── Patch Selector Header Card ───────────────────────────────────────────
lv_obj_t* createPatchSelectorCard(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav,
                                  int32_t x, int32_t y, int32_t w = 784, int32_t h = 64,
                                  PresentationLevel level = PresentationLevel::BlueprintVerbose);

// ── Virtual Keyboard (Stage / Split Zone / Harmony Visualizer) ───────────
enum class KeyboardDisplayMode : uint8_t {
    StagePlay   = 0, // 1-octave large touch keys (C..C)
    SplitZones  = 1, // 2-octave zone display showing Layer A vs Layer B ranges
    HarmonyView = 2, // 2-octave harmony display showing Scale, Input, Quantized, and Chord notes
    StageKeys2Oct = 3 // 2-octave stage play keyboard for Performance KEYS FOCUS mode
};

lv_obj_t* createVirtualKeyboard(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav,
                                KeyboardDisplayMode mode, int32_t x, int32_t y, int32_t w, int32_t h);

// Refreshes key colors and active held highlights on an existing virtual keyboard widget
// in-place without destroying or recreating the LVGL object tree.
void refreshVirtualKeyboard(lv_obj_t* keyboard_box);

// Releases any active touch notes owned by the given virtual keyboard widget
// and updates key visual states cleanly.
void releaseVirtualKeyboardNotes(lv_obj_t* keyboard_box);

// ── Step Grid (16-step touch matrix for Sequencer / Drums / Arp) ─────────
enum class StepGridKind : uint8_t {
    MelodicSequencer = 0,
    DrumTrack        = 1,
    ArpPreview       = 2
};

lv_obj_t* createStepGrid(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav,
                         StepGridKind kind, int32_t x, int32_t y, int32_t w, int32_t h);

// ── ADSR Envelope Graphical Editor ───────────────────────────────────────
lv_obj_t* createEnvelopeEditor(lv_obj_t* parent, IUiBackend& backend,
                               int32_t x, int32_t y, int32_t w, int32_t h);

// ── Modulation Matrix Editor (Paginated 4 slots per page for 800x480) ────
lv_obj_t* createModulationMatrixWidget(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav,
                                       uint8_t page, int32_t x, int32_t y, int32_t w, int32_t h);

// ── Simulated Signal Meter (with explicit DEMO / SIMULATED badge) ────────
lv_obj_t* createSignalMeter(lv_obj_t* parent, const char* title, uint8_t left_pct, uint8_t right_pct,
                            bool is_simulated, int32_t x, int32_t y, int32_t w, int32_t h);

} // namespace smk::p4::ui
