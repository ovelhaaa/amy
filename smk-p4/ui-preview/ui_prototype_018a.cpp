#include "ui_prototype_018a.h"

#include "harmony.h"
#include "performance_lock.h"
#include "ui_parameters.h"
#include "ui_theme.h"
#include "ui_widgets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace smk::p4::ui::proto018a {

namespace {

constexpr const char* kRootNames[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

constexpr const char* kScaleShort[11] = {
    "CHROM", "MAJ", "MIN", "HARM", "P.MAJ", "P.MIN", "BLUES", "DOR", "PHRY", "LYD", "MIXO"
};

constexpr const char* kDivNames[8] = {
    "1/4", "3/16.", "1/8", "1/8T", "3/32.", "1/16", "1/16T", "1/32"
};

constexpr const char* kAreaLabels[5] = {
    "TOCAR", "TIMBRES", "RITMO", "FX", "MAIS"
};

constexpr const char* kTimbresLabels[4] = {
    "Biblioteca", "Sintese", "Camadas", "Modulacao"
};

constexpr const char* kRitmoLabels[4] = {
    "Sequenciador", "Arpejador", "Padroes", "Bateria"
};

constexpr const char* kFxLabels[4] = {
    "Delay", "Reverb", "Chorus", "Master"
};

constexpr const char* kMaisLabels[3] = {
    "MIDI/SMK25", "Preferencias", "Diagnosticos"
};

constexpr const char* kWhiteNames[7] = {"C", "D", "E", "F", "G", "A", "B"};
constexpr uint8_t kWhiteOffsets[7] = {0, 2, 4, 5, 7, 9, 11};
constexpr uint8_t kBlackAfterWhite[5] = {0, 1, 3, 4, 5};
constexpr uint8_t kBlackSemis[5] = {1, 3, 6, 8, 10};

void formatNoteName(uint8_t midi_note, char* buf, size_t cap) {
    if (midi_note > 127U) {
        std::snprintf(buf, cap, "--");
        return;
    }
    const int octave = static_cast<int>(midi_note / 12U) - 1;
    std::snprintf(buf, cap, "%s%d", kRootNames[midi_note % 12U], octave);
}

bool boxesOverlap(const lv_area_t& a, const lv_area_t& b) {
    return !(a.x2 < b.x1 || b.x2 < a.x1 || a.y2 < b.y1 || b.y2 < a.y1);
}

bool objInsideScreen(const lv_obj_t* obj) {
    if (!obj || lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return false;
    lv_area_t coords{};
    lv_obj_get_coords(obj, &coords);
    return coords.x1 >= 0 && coords.y1 >= 0 && coords.x2 < kScreenWidth && coords.y2 < kScreenHeight;
}

lv_obj_t* createStyledLabel(lv_obj_t* parent, const char* text, int32_t x, int32_t y, int32_t w,
                            const lv_font_t* font, uint32_t color_hex, lv_text_align_t align = LV_TEXT_ALIGN_LEFT) {
    lv_obj_t* lbl = createLabel(parent, text, x, y, w, font, color_hex);
    if (lbl && align != LV_TEXT_ALIGN_LEFT) {
        lv_obj_set_style_text_align(lbl, align, 0);
    }
    return lbl;
}

lv_obj_t* createWrapLabel(lv_obj_t* parent, const char* text, int32_t x, int32_t y, int32_t w, uint32_t color_hex) {
    lv_obj_t* lbl = lv_label_create(parent);
    if (!lbl) return nullptr;
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(lbl, x, y);
    lv_obj_set_size(lbl, w, LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(color_hex), 0);
    lv_label_set_text(lbl, text ? text : "");
    return lbl;
}

lv_obj_t* createBadgePill(lv_obj_t* parent, const char* text, int32_t x, int32_t y,
                          int32_t w, int32_t h, uint32_t bg_hex, uint32_t txt_hex = palette::kTextPrimary) {
    lv_obj_t* box = lv_obj_create(parent);
    if (!box) return nullptr;
    lv_obj_remove_style_all(box);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, w, h);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(box, lv_color_hex(bg_hex), 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(palette::kBorder), 0);
    lv_obj_set_style_radius(box, 4, 0);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl = lv_label_create(box);
    if (lbl) {
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(lbl, std::max<int32_t>(8, w - 4));
        lv_label_set_text(lbl, text ? text : "");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(txt_hex), 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(lbl);
    }
    return box;
}

lv_obj_t* createPersistentStaticLabel(lv_obj_t* parent, const char* static_text, int32_t x, int32_t y,
                                      int32_t w, uint32_t color_hex) {
    lv_obj_t* lbl = lv_label_create(parent);
    if (!lbl) return nullptr;
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(lbl, x, y);
    lv_obj_set_width(lbl, w);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(color_hex), 0);
    lv_label_set_text_static(lbl, static_text ? static_text : "");
    return lbl;
}

void setPersistentButtonState(lv_obj_t* btn, bool active, bool accent) {
    if (!btn) return;
    if (active) lv_obj_add_state(btn, LV_STATE_CHECKED);
    else lv_obj_remove_state(btn, LV_STATE_CHECKED);
    if (!active && accent) lv_obj_add_state(btn, LV_STATE_USER_1);
    else lv_obj_remove_state(btn, LV_STATE_USER_1);
}

lv_obj_t* createPersistentButton(lv_obj_t* parent, const char* static_text, int32_t x, int32_t y,
                                 int32_t w, int32_t h, bool active = false, bool accent = false) {
    lv_obj_t* btn = lv_button_create(parent);
    if (!btn) return nullptr;
    lv_obj_remove_style_all(btn);
    lv_obj_add_style(btn, &theme().button, 0);
    lv_obj_add_style(btn, &theme().button_active, LV_STATE_CHECKED);
    lv_obj_add_style(btn, &theme().button_accent, LV_STATE_USER_1);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    setPersistentButtonState(btn, active, accent);

    lv_obj_t* lbl = lv_label_create(btn);
    if (lbl) {
        lv_label_set_text_static(lbl, static_text ? static_text : "");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_center(lbl);
    }
    return btn;
}

const char* scenePhaseText(scene::RecallPhase phase, const scene::Slot* slot) {
    switch (phase) {
        case scene::RecallPhase::WaitingBoundary: return "PENDING BAR";
        case scene::RecallPhase::WaitingPatch:    return "WAIT PATCH";
        case scene::RecallPhase::Confirmed:       return "ACTIVE";
        case scene::RecallPhase::Failed:          return "FAILED";
        case scene::RecallPhase::Cancelled:       return "CANCELLED";
        case scene::RecallPhase::PartialApply:    return "PARTIAL";
        case scene::RecallPhase::Idle:
        default:
            if (!slot || slot->state == seq::SlotState::Empty) return "EMPTY";
            if (slot->state == seq::SlotState::Corrupted) return "CORRUPT";
            if (slot->state == seq::SlotState::Incompatible) return "INCOMP";
            return "SAVED";
    }
}

uint32_t scenePhaseColor(const char* label) {
    if (!label) return palette::kPlannedBadge;
    if (std::strcmp(label, "ACTIVE") == 0) return palette::kActive;
    if (std::strncmp(label, "PENDING", 7) == 0 || std::strcmp(label, "WAIT PATCH") == 0) return palette::kAccentPrimary;
    if (std::strcmp(label, "FAILED") == 0 || std::strcmp(label, "CORRUPT") == 0) return palette::kErrorPanic;
    if (std::strcmp(label, "PARTIAL") == 0 || std::strcmp(label, "INCOMP") == 0) return palette::kWarning;
    return palette::kPlannedBadge;
}

int32_t paramScaleFactor(const ParameterDescriptor* desc) {
    if (!desc) return 1;
    if (desc->type == ParameterValueType::Float && desc->step > 0.0f && desc->step < 1.0f) {
        return 100;
    }
    return 1;
}

ParameterValue sliderRawToParam(const ParameterDescriptor& desc, int32_t raw_val) {
    const int32_t scale = paramScaleFactor(&desc);
    if (desc.type == ParameterValueType::Float) {
        const float f = std::clamp(static_cast<float>(raw_val) / static_cast<float>(scale),
                                   desc.minimum, desc.maximum);
        return ParameterValue::makeFloat(f);
    }
    if (desc.type == ParameterValueType::Enum) {
        return ParameterValue::makeEnum(raw_val);
    }
    if (desc.type == ParameterValueType::Bool) {
        return ParameterValue::makeBool(raw_val != 0);
    }
    return ParameterValue::makeInt(raw_val);
}

int32_t paramToSliderRaw(const ParameterDescriptor& desc, const ParameterValue& val) {
    const int32_t scale = paramScaleFactor(&desc);
    if (desc.type == ParameterValueType::Float) {
        return static_cast<int32_t>(std::lround(val.asFloat() * static_cast<float>(scale)));
    }
    return val.asInt();
}

// Local self-contained 2-octave Virtual Touch Keyboard for Tocar->Teclado.
// Releases ONLY its own active touch notes on LV_EVENT_DELETE, never interfering
// with USB MIDI notes or sequencer/arp engines when navigating elsewhere.
struct ProtoKeySlot {
    lv_obj_t* btn = nullptr;
    uint8_t semi = 0;
    uint8_t active_note = 60;
    uint32_t active_token = 0;
    bool pressed = false;
    bool is_black = false;
};

struct ProtoKeyboardState {
    IUiBackend* backend = nullptr;
    lv_obj_t* box = nullptr;
    lv_obj_t* info_lbl = nullptr;
    ProtoKeySlot keys[25]{};
    uint8_t key_count = 0;
};

void refreshProtoKeyboardColors(ProtoKeyboardState& st) {
    if (!st.backend) return;
    const int32_t oct_shift = std::clamp<int32_t>(st.backend->model().getInt("perf.octave_shift", 0), -2, 2);
    const uint8_t start_note = static_cast<uint8_t>(std::clamp<int32_t>(48 + oct_shift * 12, 24, 84));
    const auto& hc = st.backend->model().live_harmony.configuration;
    const auto mask_info = harmony::transform(start_note, hc);
    const uint16_t mask = mask_info.mask;

    for (uint8_t i = 0; i < st.key_count; ++i) {
        ProtoKeySlot& k = st.keys[i];
        if (!k.btn) continue;
        const uint8_t note = static_cast<uint8_t>(start_note + k.semi);
        const bool held = k.pressed || st.backend->isNoteHeld(note);
        const bool in_scale = !hc.scale_on || ((mask & (1u << (note % 12U))) != 0U);
        uint32_t col = k.is_black ? palette::kKeyBlack : palette::kKeyWhite;
        if (held) {
            col = palette::kAccentPrimary;
        } else if (hc.scale_on && in_scale && !k.is_black) {
            col = 0xE6F3ED;
        } else if (hc.scale_on && !in_scale) {
            col = k.is_black ? 0x1D232A : 0x8D9AA4;
        }
        lv_obj_set_style_bg_color(k.btn, lv_color_hex(col), 0);
    }
}

void releaseProtoKeyboardSlots(ProtoKeyboardState& st) {
    if (!st.backend) return;
    for (uint8_t i = 0; i < st.key_count; ++i) {
        ProtoKeySlot& k = st.keys[i];
        if (k.pressed) {
            st.backend->requestNoteOff(k.active_note, k.active_token);
            k.pressed = false;
            k.active_token = 0;
        }
    }
}

struct ProtoKeyEventCtx {
    ProtoKeyboardState* owner = nullptr;
    uint8_t index = 0;
};

void onProtoKeyEvent(lv_event_t* e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST) {
        return;
    }
    auto* ctx = static_cast<ProtoKeyEventCtx*>(lv_event_get_user_data(e));
    if (!ctx || !ctx->owner || !ctx->owner->backend || ctx->index >= ctx->owner->key_count) return;
    ProtoKeyboardState& st = *ctx->owner;
    ProtoKeySlot& k = st.keys[ctx->index];
    const int32_t oct_shift = std::clamp<int32_t>(st.backend->model().getInt("perf.octave_shift", 0), -2, 2);
    const uint8_t start_note = static_cast<uint8_t>(std::clamp<int32_t>(48 + oct_shift * 12, 24, 84));

    if (code == LV_EVENT_PRESSED) {
        if (k.pressed) return;
        const uint8_t note = static_cast<uint8_t>(start_note + k.semi);
        const UiCommandResult res = st.backend->requestNoteOn(note, 100);
        if (res.accepted()) {
            k.pressed = true;
            k.active_note = note;
            k.active_token = res.token;
        }
        st.backend->model().setLastPreviewNote(note);
        if (st.info_lbl) {
            char nbuf[12];
            formatNoteName(note, nbuf, sizeof(nbuf));
            char msg[96];
            std::snprintf(msg, sizeof(msg), "ULTIMA TECLA TOUCH: %s (MIDI %u) -> %s",
                          nbuf, unsigned(note), res.notice ? res.notice : commandStateLabel(res.state));
            lv_label_set_text(st.info_lbl, msg);
        }
        refreshProtoKeyboardColors(st);
    } else {
        if (!k.pressed) return;
        const uint8_t n = k.active_note;
        const uint32_t tok = k.active_token;
        k.pressed = false;
        k.active_token = 0;
        st.backend->requestNoteOff(n, tok);
        refreshProtoKeyboardColors(st);
    }
}

}  // namespace

UiShell018A::UiShell018A(IUiBackend& backend)
    : backend_(backend) {}

UiShell018A::~UiShell018A() {
    teardown();
}

bool UiShell018A::begin(lv_obj_t* root_screen) {
    if (initialized_) return true;
    theme().init();

    if (root_screen) {
        root_ = root_screen;
        owned_root_ = false;
    } else {
        root_ = lv_obj_create(nullptr);
        if (!root_) return false;
        owned_root_ = true;
        lv_screen_load(root_);
    }

    applyScreenStyle(root_);
    lv_obj_set_size(root_, kScreenWidth, kScreenHeight);
    lv_obj_set_pos(root_, 0, 0);
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    last_seen_revision_ = 0U;
    buildShell();
    initialized_ = true;
    metrics_.live_objects = countAllObjects();
    return true;
}

void UiShell018A::teardown() {
    if (!initialized_ && !root_) return;
    if (active_area_ == AreaId::Tocar && tocar_mode_ == TocarMode::VirtualKeys) {
        backend_.releaseAllUiNotes();
        ++metrics_.ui_note_releases;
    }

    if (root_) {
        if (owned_root_) {
            lv_obj_t* fallback = lv_obj_create(nullptr);
            if (fallback) {
                lv_screen_load(fallback);
            }
            lv_obj_delete(root_);
        } else {
            lv_obj_clean(root_);
        }
    }

    root_ = nullptr;
    header_ = nullptr;
    sidebar_ = nullptr;
    panel_ = nullptr;
    footer_ = nullptr;
    overlay_backdrop_ = nullptr;
    overlay_card_ = nullptr;
    hdr_mode_badge_ = nullptr;
    hdr_mode_label_ = nullptr;
    hdr_patch_label_ = nullptr;
    hdr_scene_label_ = nullptr;
    hdr_bpm_btn_ = nullptr;
    hdr_bpm_label_ = nullptr;
    hdr_harm_btn_ = nullptr;
    hdr_harm_label_ = nullptr;
    hdr_q_btn_ = nullptr;
    hdr_q_label_ = nullptr;
    hdr_ch_btn_ = nullptr;
    hdr_ch_label_ = nullptr;
    hdr_panic_btn_ = nullptr;
    hdr_variant_btn_ = nullptr;
    hdr_row2_info_label_ = nullptr;
    footer_btns_.fill(nullptr);
    sidebar_btns_.fill(nullptr);
    tracked_params_.fill(TrackedParamWidget{});
    tracked_param_count_ = 0U;
    inspector_title_ = nullptr;
    inspector_values_ = nullptr;
    inspector_ack_ = nullptr;
    inspector_minus_btn_ = nullptr;
    inspector_plus_btn_ = nullptr;
    tocar_arp_pill_ = nullptr;
    tocar_seq_pill_ = nullptr;
    tocar_fx_pill_ = nullptr;
    tocar_harm_pill_ = nullptr;
    tocar_vkbd_ = nullptr;
    tocar_vkbd_info_ = nullptr;
    overlay_status_label_ = nullptr;
    overlay_q_btn_ = nullptr;
    overlay_ch_btn_ = nullptr;
    dynamic_feedback_label_ = nullptr;
    delay_status_label_ = nullptr;
    diag_live_label_ = nullptr;
    initialized_ = false;
    owned_root_ = false;
}

void UiShell018A::service(uint32_t now_ms) {
    if (!initialized_) return;
    last_service_ms_ = now_ms;
    backend_.service(now_ms);
    if (!dragging_slider_) {
        refreshDynamicLabels();
    } else {
        refreshHeaderDynamic();
    }
    ++metrics_.inplace_updates;
    metrics_.live_objects = countAllObjects();
}

void UiShell018A::selectArea(AreaId area) {
    if (area >= AreaId::Count) return;
    const AreaId prev_area = active_area_;
    const TocarMode prev_mode = tocar_mode_;
    if (active_overlay_ != OverlayKind::None) {
        closeOverlay();
    }
    if (prev_area == area) {
        refreshDynamicLabels();
        return;
    }
    maybeReleaseVirtualKeys(prev_area, prev_mode);
    active_area_ = area;
    updateFooterVisuals();
    updateGeometryForArea();
    if (panel_) lv_obj_clean(panel_);
    if (sidebar_) lv_obj_clean(sidebar_);
    rebuildSidebar();
    rebuildActivePanel();
    refreshHeaderDynamic();
    metrics_.live_objects = countAllObjects();
}

void UiShell018A::selectTocarMode(TocarMode mode) {
    if (mode >= TocarMode::Count) return;
    const AreaId prev_area = active_area_;
    const TocarMode prev_mode = tocar_mode_;
    if (prev_area != AreaId::Tocar) {
        maybeReleaseVirtualKeys(prev_area, prev_mode);
        active_area_ = AreaId::Tocar;
        tocar_mode_ = mode;
        updateFooterVisuals();
        updateGeometryForArea();
        if (panel_) lv_obj_clean(panel_);
        if (sidebar_) lv_obj_clean(sidebar_);
        rebuildSidebar();
        rebuildActivePanel();
    } else if (prev_mode != mode) {
        maybeReleaseVirtualKeys(prev_area, prev_mode);
        tocar_mode_ = mode;
        rebuildActivePanel();
    }
    refreshHeaderDynamic();
    metrics_.live_objects = countAllObjects();
}

void UiShell018A::selectTimbresModule(TimbresModule mod) {
    if (mod >= TimbresModule::Count) return;
    const AreaId prev_area = active_area_;
    const TocarMode prev_mode = tocar_mode_;
    if (prev_area != AreaId::Timbres) {
        maybeReleaseVirtualKeys(prev_area, prev_mode);
        active_area_ = AreaId::Timbres;
        timbres_module_ = mod;
        updateFooterVisuals();
        updateGeometryForArea();
        if (panel_) lv_obj_clean(panel_);
        if (sidebar_) lv_obj_clean(sidebar_);
        rebuildSidebar();
        rebuildActivePanel();
    } else if (timbres_module_ != mod) {
        timbres_module_ = mod;
        if (panel_) lv_obj_clean(panel_);
        if (sidebar_) lv_obj_clean(sidebar_);
        rebuildSidebar();
        rebuildActivePanel();
    }
    metrics_.live_objects = countAllObjects();
}

void UiShell018A::selectRitmoModule(RitmoModule mod) {
    if (mod >= RitmoModule::Count) return;
    const AreaId prev_area = active_area_;
    const TocarMode prev_mode = tocar_mode_;
    if (prev_area != AreaId::Ritmo) {
        maybeReleaseVirtualKeys(prev_area, prev_mode);
        active_area_ = AreaId::Ritmo;
        ritmo_module_ = mod;
        updateFooterVisuals();
        updateGeometryForArea();
        if (panel_) lv_obj_clean(panel_);
        if (sidebar_) lv_obj_clean(sidebar_);
        rebuildSidebar();
        rebuildActivePanel();
    } else if (ritmo_module_ != mod) {
        ritmo_module_ = mod;
        if (panel_) lv_obj_clean(panel_);
        if (sidebar_) lv_obj_clean(sidebar_);
        rebuildSidebar();
        rebuildActivePanel();
    }
    metrics_.live_objects = countAllObjects();
}

void UiShell018A::selectSeqViewMode(SeqViewMode mode) {
    if (seq_view_mode_ == mode && active_area_ == AreaId::Ritmo && ritmo_module_ == RitmoModule::Sequenciador) {
        return;
    }
    seq_view_mode_ = mode;
    if (active_area_ == AreaId::Ritmo && ritmo_module_ == RitmoModule::Sequenciador) {
        rebuildActivePanel();
    }
    metrics_.live_objects = countAllObjects();
}

void UiShell018A::selectFxModule(FxModule mod) {
    if (mod >= FxModule::Count) return;
    const AreaId prev_area = active_area_;
    const TocarMode prev_mode = tocar_mode_;
    if (prev_area != AreaId::Fx) {
        maybeReleaseVirtualKeys(prev_area, prev_mode);
        active_area_ = AreaId::Fx;
        fx_module_ = mod;
        updateFooterVisuals();
        updateGeometryForArea();
        if (panel_) lv_obj_clean(panel_);
        if (sidebar_) lv_obj_clean(sidebar_);
        rebuildSidebar();
        rebuildActivePanel();
    } else if (fx_module_ != mod) {
        fx_module_ = mod;
        if (panel_) lv_obj_clean(panel_);
        if (sidebar_) lv_obj_clean(sidebar_);
        rebuildSidebar();
        rebuildActivePanel();
    }
    metrics_.live_objects = countAllObjects();
}

void UiShell018A::selectMaisModule(MaisModule mod) {
    if (mod >= MaisModule::Count) return;
    const AreaId prev_area = active_area_;
    const TocarMode prev_mode = tocar_mode_;
    if (prev_area != AreaId::Mais) {
        maybeReleaseVirtualKeys(prev_area, prev_mode);
        active_area_ = AreaId::Mais;
        mais_module_ = mod;
        updateFooterVisuals();
        updateGeometryForArea();
        if (panel_) lv_obj_clean(panel_);
        if (sidebar_) lv_obj_clean(sidebar_);
        rebuildSidebar();
        rebuildActivePanel();
    } else if (mais_module_ != mod) {
        mais_module_ = mod;
        if (panel_) lv_obj_clean(panel_);
        if (sidebar_) lv_obj_clean(sidebar_);
        rebuildSidebar();
        rebuildActivePanel();
    }
    metrics_.live_objects = countAllObjects();
}

void UiShell018A::selectSynthSubTab(SynthSubTab tab) {
    if (tab >= SynthSubTab::Count) return;
    synth_sub_tab_ = tab;
    if (active_area_ == AreaId::Timbres && timbres_module_ == TimbresModule::Sintese) {
        rebuildActivePanel();
    }
    metrics_.live_objects = countAllObjects();
}

void UiShell018A::setHeaderVariant(HeaderVariant variant) {
    if (header_variant_ == variant) return;
    header_variant_ = variant;
    const OverlayKind reopen_overlay = active_overlay_;
    if (overlay_backdrop_) {
        overlay_status_label_ = nullptr;
        overlay_q_btn_ = nullptr;
        overlay_ch_btn_ = nullptr;
        lv_obj_delete(overlay_backdrop_);
        overlay_backdrop_ = nullptr;
        overlay_card_ = nullptr;
    }
    if (panel_) lv_obj_clean(panel_);
    if (sidebar_) lv_obj_clean(sidebar_);
    if (header_) lv_obj_clean(header_);
    rebuildHeader();
    updateGeometryForArea();
    rebuildSidebar();
    rebuildActivePanel();
    if (reopen_overlay != OverlayKind::None) {
        active_overlay_ = reopen_overlay;
        rebuildOverlay();
    }
    metrics_.live_objects = countAllObjects();
}

void UiShell018A::openOverlay(OverlayKind overlay, uint8_t scene_slot) {
    if (overlay == OverlayKind::None) {
        closeOverlay();
        return;
    }
    if (scene_slot < scene::kSlots) {
        selected_scene_slot_ = scene_slot;
    }
    active_overlay_ = overlay;
    rebuildOverlay();
    metrics_.live_objects = countAllObjects();
}

void UiShell018A::closeOverlay() {
    if (active_overlay_ == OverlayKind::None && !overlay_backdrop_) return;
    active_overlay_ = OverlayKind::None;
    overlay_status_label_ = nullptr;
    overlay_q_btn_ = nullptr;
    overlay_ch_btn_ = nullptr;
    if (overlay_backdrop_) {
        lv_obj_delete(overlay_backdrop_);
        overlay_backdrop_ = nullptr;
        overlay_card_ = nullptr;
    }
    refreshDynamicLabels();
    metrics_.live_objects = countAllObjects();
}

void UiShell018A::selectParameter(const char* id) {
    if (id && ParameterRegistry::findById(id)) {
        selected_param_id_ = id;
    }
    refreshDynamicLabels();
}

void UiShell018A::stepSelectedParameter(int32_t steps) {
    if (!selected_param_id_) return;
    const ParameterDescriptor* desc = ParameterRegistry::findById(selected_param_id_);
    if (!desc || !backend_.isParameterEditable(desc->id)) {
        status_flash_text_ = "BLOQUEADO / NAO EDITAVEL";
        refreshDynamicLabels();
        return;
    }
    ParameterValue cur{};
    if (!backend_.model().getValue(desc->id, cur)) return;
    UiCommandResult res{};
    if (desc->type == ParameterValueType::Float) {
        const float next = std::clamp(cur.asFloat() + static_cast<float>(steps) * desc->step,
                                      desc->minimum, desc->maximum);
        res = backend_.requestParameterFloat(desc->id, next);
    } else if (desc->type == ParameterValueType::Bool) {
        res = backend_.requestParameterBool(desc->id, !cur.asBool());
    } else {
        const int32_t step_i = std::max<int32_t>(1, static_cast<int32_t>(desc->step));
        const int32_t min_i = static_cast<int32_t>(desc->minimum);
        const int32_t max_i = static_cast<int32_t>(desc->maximum);
        const int32_t next = std::clamp<int32_t>(cur.asInt() + steps * step_i, min_i, max_i);
        res = (desc->type == ParameterValueType::Enum)
                  ? backend_.requestParameterValue(desc->id, ParameterValue::makeEnum(next))
                  : backend_.requestParameterInt(desc->id, next);
    }
    status_flash_text_ = res.notice ? res.notice : commandStateLabel(res.state);
    refreshDynamicLabels();
}

void UiShell018A::selectSceneSlot(uint8_t slot_index) {
    if (slot_index < scene::kSlots) {
        selected_scene_slot_ = slot_index;
        if (active_area_ == AreaId::Tocar) {
            rebuildActivePanel();
        }
    }
}

void UiShell018A::selectSeqStep(uint8_t step_index) {
    if (step_index < seq::kSteps) {
        selected_seq_step_ = step_index;
        if (active_area_ == AreaId::Ritmo && ritmo_module_ == RitmoModule::Sequenciador) {
            rebuildActivePanel();
        }
    }
}

UiShell018A::ControlBinding* UiShell018A::makeBinding(ControlTag tag, int32_t arg0, int32_t arg1, const char* param_id) {
    auto* b = static_cast<ControlBinding*>(lv_malloc(sizeof(ControlBinding)));
    if (!b) return nullptr;
    b->self = this;
    b->tag = tag;
    b->arg0 = arg0;
    b->arg1 = arg1;
    b->param_id = param_id;
    return b;
}

void UiShell018A::attachClick(lv_obj_t* obj, ControlTag tag, int32_t arg0, int32_t arg1, const char* param_id) {
    if (!obj) return;
    ControlBinding* b = makeBinding(tag, arg0, arg1, param_id);
    if (!b) return;
    lv_obj_add_event_cb(obj, onButtonClicked, LV_EVENT_CLICKED, b);
    lv_obj_add_event_cb(obj, onControlDeleted, LV_EVENT_DELETE, b);
}

void UiShell018A::attachValueChanged(lv_obj_t* slider, const char* param_id) {
    if (!slider) return;
    ControlBinding* b = makeBinding(ControlTag::ParamSlider, 0, 0, param_id);
    if (!b) return;
    lv_obj_add_event_cb(slider, onSliderChanged, LV_EVENT_VALUE_CHANGED, b);
    lv_obj_add_event_cb(slider, onSliderReleased, LV_EVENT_RELEASED, b);
    lv_obj_add_event_cb(slider, onControlDeleted, LV_EVENT_DELETE, b);
}

void UiShell018A::onControlDeleted(lv_event_t* event) {
    if (lv_event_get_code(event) != LV_EVENT_DELETE) return;
    void* data = lv_event_get_user_data(event);
    if (data) lv_free(data);
}

void UiShell018A::onButtonClicked(lv_event_t* event) {
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    auto* b = static_cast<ControlBinding*>(lv_event_get_user_data(event));
    if (!b || !b->self) return;
    b->self->handleBindingAction(*b);
}

void UiShell018A::onSliderChanged(lv_event_t* event) {
    if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) return;
    auto* b = static_cast<ControlBinding*>(lv_event_get_user_data(event));
    if (!b || !b->self || !b->param_id) return;
    UiShell018A& self = *b->self;
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(event));
    const ParameterDescriptor* desc = ParameterRegistry::findById(b->param_id);
    if (!desc || !slider) return;

    self.dragging_slider_ = true;
    self.selected_param_id_ = b->param_id;
    const int32_t raw = lv_slider_get_value(slider);
    const ParameterValue local_pv = sliderRawToParam(*desc, raw);

    for (uint8_t i = 0; i < self.tracked_param_count_; ++i) {
        if (self.tracked_params_[i].id && std::strcmp(self.tracked_params_[i].id, b->param_id) == 0) {
            if (self.tracked_params_[i].value_label) {
                char vbuf[40];
                ParameterRegistry::formatValue(*desc, local_pv, vbuf, sizeof(vbuf));
                lv_label_set_text(self.tracked_params_[i].value_label, vbuf);
            }
            if (self.tracked_params_[i].status_label) {
                lv_label_set_text(self.tracked_params_[i].status_label, "LOCAL DRAG");
            }
            break;
        }
    }

    // Throttle owner commands during continuous drag (>= 35ms) and always commit on RELEASED.
    if (self.last_service_ms_ - self.last_slider_drag_send_ms_ >= 35U || self.last_slider_drag_send_ms_ == 0U) {
        self.last_slider_drag_send_ms_ = self.last_service_ms_;
        const UiCommandResult res = self.backend_.requestParameterValue(b->param_id, local_pv);
        if (res.state == UiCommandState::Rejected || res.state == UiCommandState::Unsupported) {
            self.status_flash_text_ = res.notice ? res.notice : commandStateLabel(res.state);
        }
    }
}

void UiShell018A::onSliderReleased(lv_event_t* event) {
    if (lv_event_get_code(event) != LV_EVENT_RELEASED) return;
    auto* b = static_cast<ControlBinding*>(lv_event_get_user_data(event));
    if (!b || !b->self || !b->param_id) return;
    UiShell018A& self = *b->self;
    self.dragging_slider_ = false;
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(event));
    const ParameterDescriptor* desc = ParameterRegistry::findById(b->param_id);
    if (!desc || !slider) return;

    const ParameterValue pv = sliderRawToParam(*desc, lv_slider_get_value(slider));
    const UiCommandResult res = self.backend_.requestParameterValue(b->param_id, pv);
    self.status_flash_text_ = res.notice ? res.notice : commandStateLabel(res.state);
    self.refreshDynamicLabels();
}

void UiShell018A::buildShell() {
    ++metrics_.shell_builds;
    footer_ = lv_obj_create(root_);
    header_ = lv_obj_create(root_);
    sidebar_ = lv_obj_create(root_);
    panel_ = lv_obj_create(root_);

    for (lv_obj_t* container : {footer_, header_, sidebar_, panel_}) {
        lv_obj_remove_style_all(container);
        lv_obj_remove_flag(container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_opa(container, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(container, lv_color_hex(palette::kBackground), 0);
    }

    // Build immutable footer first so long-lived allocations sit together at the bottom of the TLSF pool.
    buildFooter();
    rebuildHeader();
    updateFooterVisuals();
    updateGeometryForArea();
    rebuildSidebar();
    rebuildActivePanel();
    refreshDynamicLabels();
}

void UiShell018A::rebuildHeader() {
    if (!header_) return;
    lv_obj_clean(header_);
    ++metrics_.header_rebuilds;

    hdr_mode_badge_ = nullptr;
    hdr_mode_label_ = nullptr;
    hdr_patch_label_ = nullptr;
    hdr_scene_label_ = nullptr;
    hdr_bpm_btn_ = nullptr;
    hdr_bpm_label_ = nullptr;
    hdr_harm_btn_ = nullptr;
    hdr_harm_label_ = nullptr;
    hdr_q_btn_ = nullptr;
    hdr_q_label_ = nullptr;
    hdr_ch_btn_ = nullptr;
    hdr_ch_label_ = nullptr;
    hdr_panic_btn_ = nullptr;
    hdr_variant_btn_ = nullptr;
    hdr_row2_info_label_ = nullptr;

    const int32_t hdr_h = (header_variant_ == HeaderVariant::CompactOverlay) ? 44 : 68;
    lv_obj_set_pos(header_, 0, 0);
    lv_obj_set_size(header_, kScreenWidth, hdr_h);
    lv_obj_set_style_bg_color(header_, lv_color_hex(0x0D141B), 0);
    lv_obj_set_style_border_width(header_, 1, 0);
    lv_obj_set_style_border_side(header_, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(header_, lv_color_hex(palette::kBorder), 0);

    if (header_variant_ == HeaderVariant::CompactOverlay) {
        // Variant A (800x44): disjoint targets, >= 4px gaps, all interactive buttons h=36
        hdr_mode_badge_ = lv_obj_create(header_);
        lv_obj_remove_style_all(hdr_mode_badge_);
        lv_obj_set_pos(hdr_mode_badge_, 4, 4);
        lv_obj_set_size(hdr_mode_badge_, 92, 36);
        lv_obj_set_style_bg_opa(hdr_mode_badge_, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(hdr_mode_badge_, lv_color_hex(backend_.isDemoMode() ? palette::kWarning : palette::kActive), 0);
        lv_obj_set_style_border_width(hdr_mode_badge_, 1, 0);
        lv_obj_set_style_border_color(hdr_mode_badge_, lv_color_hex(palette::kBorder), 0);
        lv_obj_set_style_radius(hdr_mode_badge_, 4, 0);
        lv_obj_remove_flag(hdr_mode_badge_, LV_OBJ_FLAG_SCROLLABLE);
        hdr_mode_label_ = lv_label_create(hdr_mode_badge_);
        lv_label_set_text_static(hdr_mode_label_, backend_.isDemoMode() ? "DEMO N/OP" : "P4 LIVE");
        lv_obj_set_style_text_font(hdr_mode_label_, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(hdr_mode_label_, lv_color_hex(palette::kBackground), 0);
        lv_obj_center(hdr_mode_label_);

        lv_obj_t* ps_box = lv_obj_create(header_);
        applyPanelStyle(ps_box, false);
        lv_obj_set_pos(ps_box, 100, 4);
        lv_obj_set_size(ps_box, 200, 36);
        lv_obj_set_style_pad_all(ps_box, 2, 0);
        std::snprintf(hdr_patch_buf_, sizeof(hdr_patch_buf_), "#001");
        std::snprintf(hdr_scene_buf_, sizeof(hdr_scene_buf_), "SCENE: --");
        hdr_patch_label_ = createPersistentStaticLabel(ps_box, hdr_patch_buf_, 4, 1, 190, palette::kTextPrimary);
        hdr_scene_label_ = createPersistentStaticLabel(ps_box, hdr_scene_buf_, 4, 17, 190, palette::kTextSecondary);

        std::snprintf(hdr_bpm_buf_, sizeof(hdr_bpm_buf_), "120 BPM");
        hdr_bpm_btn_ = createPersistentButton(header_, hdr_bpm_buf_, 304, 4, 96, 36, false, false);
        hdr_bpm_label_ = hdr_bpm_btn_ ? lv_obj_get_child(hdr_bpm_btn_, 0) : nullptr;
        attachClick(hdr_bpm_btn_, ControlTag::HeaderBpmBtn);

        std::snprintf(hdr_harm_buf_, sizeof(hdr_harm_buf_), "C MAJ");
        hdr_harm_btn_ = createPersistentButton(header_, hdr_harm_buf_, 404, 4, 126, 36, false, false);
        hdr_harm_label_ = hdr_harm_btn_ ? lv_obj_get_child(hdr_harm_btn_, 0) : nullptr;
        attachClick(hdr_harm_btn_, ControlTag::HeaderHarmBtn);

        hdr_q_btn_ = createPersistentButton(header_, "Q OFF", 534, 4, 54, 36, false, false);
        hdr_q_label_ = hdr_q_btn_ ? lv_obj_get_child(hdr_q_btn_, 0) : nullptr;
        attachClick(hdr_q_btn_, ControlTag::HeaderQBtn);

        hdr_ch_btn_ = createPersistentButton(header_, "CH OFF", 592, 4, 58, 36, false, false);
        hdr_ch_label_ = hdr_ch_btn_ ? lv_obj_get_child(hdr_ch_btn_, 0) : nullptr;
        attachClick(hdr_ch_btn_, ControlTag::HeaderChBtn);

        hdr_variant_btn_ = createPersistentButton(header_, "HDR A", 654, 4, 64, 36, false, true);
        attachClick(hdr_variant_btn_, ControlTag::HeaderVariantBtn);

        hdr_panic_btn_ = createPersistentButton(header_, "PANIC", 722, 4, 74, 36, false, false);
        applyPanicButtonStyle(hdr_panic_btn_);
        attachClick(hdr_panic_btn_, ControlTag::HeaderPanicBtn);
    } else {
        // Variant B (800x68): two 30px rows + 82x60 right Panic column, zero overlap
        hdr_mode_badge_ = lv_obj_create(header_);
        lv_obj_remove_style_all(hdr_mode_badge_);
        lv_obj_set_pos(hdr_mode_badge_, 4, 3);
        lv_obj_set_size(hdr_mode_badge_, 92, 30);
        lv_obj_set_style_bg_opa(hdr_mode_badge_, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(hdr_mode_badge_, lv_color_hex(backend_.isDemoMode() ? palette::kWarning : palette::kActive), 0);
        lv_obj_set_style_border_width(hdr_mode_badge_, 1, 0);
        lv_obj_set_style_border_color(hdr_mode_badge_, lv_color_hex(palette::kBorder), 0);
        lv_obj_set_style_radius(hdr_mode_badge_, 4, 0);
        lv_obj_remove_flag(hdr_mode_badge_, LV_OBJ_FLAG_SCROLLABLE);
        hdr_mode_label_ = lv_label_create(hdr_mode_badge_);
        lv_label_set_text_static(hdr_mode_label_, backend_.isDemoMode() ? "DEMO N/OP" : "P4 LIVE");
        lv_obj_set_style_text_font(hdr_mode_label_, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(hdr_mode_label_, lv_color_hex(palette::kBackground), 0);
        lv_obj_center(hdr_mode_label_);

        lv_obj_t* ps_box = lv_obj_create(header_);
        applyPanelStyle(ps_box, false);
        lv_obj_set_pos(ps_box, 100, 3);
        lv_obj_set_size(ps_box, 292, 30);
        lv_obj_set_style_pad_all(ps_box, 2, 0);
        std::snprintf(hdr_patch_buf_, sizeof(hdr_patch_buf_), "#001");
        hdr_patch_label_ = createPersistentStaticLabel(ps_box, hdr_patch_buf_, 4, 4, 280, palette::kTextPrimary);

        std::snprintf(hdr_bpm_buf_, sizeof(hdr_bpm_buf_), "120 BPM");
        hdr_bpm_btn_ = createPersistentButton(header_, hdr_bpm_buf_, 396, 3, 104, 30, false, false);
        hdr_bpm_label_ = hdr_bpm_btn_ ? lv_obj_get_child(hdr_bpm_btn_, 0) : nullptr;
        attachClick(hdr_bpm_btn_, ControlTag::HeaderBpmBtn);

        hdr_variant_btn_ = createPersistentButton(header_, "HDR B>A", 504, 3, 76, 30, false, true);
        attachClick(hdr_variant_btn_, ControlTag::HeaderVariantBtn);

        lv_obj_t* sc_box = lv_obj_create(header_);
        applyPanelStyle(sc_box, false);
        lv_obj_set_pos(sc_box, 584, 3);
        lv_obj_set_size(sc_box, 126, 30);
        lv_obj_set_style_pad_all(sc_box, 2, 0);
        std::snprintf(hdr_scene_buf_, sizeof(hdr_scene_buf_), "SC:--");
        hdr_scene_label_ = createPersistentStaticLabel(sc_box, hdr_scene_buf_, 4, 4, 118, palette::kAccentPrimary);

        std::snprintf(hdr_harm_buf_, sizeof(hdr_harm_buf_), "C MAJ [NEAR UP]");
        hdr_harm_btn_ = createPersistentButton(header_, hdr_harm_buf_, 4, 36, 206, 30, false, false);
        hdr_harm_label_ = hdr_harm_btn_ ? lv_obj_get_child(hdr_harm_btn_, 0) : nullptr;
        attachClick(hdr_harm_btn_, ControlTag::HeaderHarmBtn);

        hdr_q_btn_ = createPersistentButton(header_, "Q OFF", 214, 36, 82, 30, false, false);
        hdr_q_label_ = hdr_q_btn_ ? lv_obj_get_child(hdr_q_btn_, 0) : nullptr;
        attachClick(hdr_q_btn_, ControlTag::HeaderQBtn);

        hdr_ch_btn_ = createPersistentButton(header_, "CH OFF", 300, 36, 116, 30, false, false);
        hdr_ch_label_ = hdr_ch_btn_ ? lv_obj_get_child(hdr_ch_btn_, 0) : nullptr;
        attachClick(hdr_ch_btn_, ControlTag::HeaderChBtn);

        lv_obj_t* info_box = lv_obj_create(header_);
        applyPanelStyle(info_box, false);
        lv_obj_set_pos(info_box, 420, 36);
        lv_obj_set_size(info_box, 290, 30);
        lv_obj_set_style_pad_all(info_box, 2, 0);
        std::snprintf(hdr_row2_buf_, sizeof(hdr_row2_buf_), "USB Ch1+Arp+Touch | Seq quant indep.");
        hdr_row2_info_label_ = createPersistentStaticLabel(info_box, hdr_row2_buf_, 4, 4, 280, palette::kTextSecondary);

        hdr_panic_btn_ = createPersistentButton(header_, "PANIC", 714, 4, 82, 60, false, false);
        applyPanicButtonStyle(hdr_panic_btn_);
        attachClick(hdr_panic_btn_, ControlTag::HeaderPanicBtn);
    }

    metrics_.header_objects = countSubtree(header_);
    refreshHeaderDynamic();
    if (root_) lv_obj_update_layout(root_);
}

void UiShell018A::buildFooter() {
    if (!footer_) return;
    lv_obj_clean(footer_);
    lv_obj_set_pos(footer_, 0, kScreenHeight - 48);
    lv_obj_set_size(footer_, kScreenWidth, 48);
    lv_obj_set_style_bg_color(footer_, lv_color_hex(0x0D141B), 0);
    lv_obj_set_style_border_width(footer_, 1, 0);
    lv_obj_set_style_border_side(footer_, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(footer_, lv_color_hex(palette::kBorder), 0);

    for (uint8_t i = 0; i < 5; ++i) {
        const int32_t x = 8 + static_cast<int32_t>(i) * 158;
        lv_obj_t* btn = createPersistentButton(footer_, kAreaLabels[i], x, 4, 150, 40, false, false);
        footer_btns_[i] = btn;
        attachClick(btn, ControlTag::FooterArea, static_cast<int32_t>(i));
    }
    metrics_.footer_objects = countSubtree(footer_);
}

void UiShell018A::updateFooterVisuals() {
    for (uint8_t i = 0; i < 5; ++i) {
        if (footer_btns_[i]) {
            const bool active = (static_cast<uint8_t>(active_area_) == i);
            setPersistentButtonState(footer_btns_[i], active, active);
        }
    }
}

void UiShell018A::updateGeometryForArea() {
    const int32_t hdr_h = (header_variant_ == HeaderVariant::CompactOverlay) ? 44 : 68;
    const int32_t content_h = (kScreenHeight - 48) - hdr_h;

    if (active_area_ == AreaId::Tocar) {
        lv_obj_add_flag(sidebar_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(sidebar_, 0, hdr_h);
        lv_obj_set_size(sidebar_, 0, content_h);

        lv_obj_set_pos(panel_, 0, hdr_h);
        lv_obj_set_size(panel_, kScreenWidth, content_h);
    } else {
        lv_obj_remove_flag(sidebar_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(sidebar_, 0, hdr_h);
        lv_obj_set_size(sidebar_, 160, content_h);
        lv_obj_set_style_bg_color(sidebar_, lv_color_hex(0x131D26), 0);
        lv_obj_set_style_border_width(sidebar_, 1, 0);
        lv_obj_set_style_border_side(sidebar_, LV_BORDER_SIDE_RIGHT, 0);
        lv_obj_set_style_border_color(sidebar_, lv_color_hex(palette::kBorder), 0);

        lv_obj_set_pos(panel_, 160, hdr_h);
        lv_obj_set_size(panel_, kScreenWidth - 160, content_h);
    }
    if (root_) lv_obj_update_layout(root_);
}

void UiShell018A::rebuildSidebar() {
    if (!sidebar_) return;
    lv_obj_clean(sidebar_);
    sidebar_btns_.fill(nullptr);
    if (active_area_ == AreaId::Tocar) {
        return;
    }
    ++metrics_.sidebar_rebuilds;

    const char* area_title = kAreaLabels[static_cast<uint8_t>(active_area_)];
    createStyledLabel(sidebar_, area_title, 10, 6, 140, &lv_font_montserrat_14, palette::kAccentPrimary);

    const char* const* labels = nullptr;
    uint8_t count = 0U;
    uint8_t selected = 0U;

    switch (active_area_) {
        case AreaId::Timbres:
            labels = kTimbresLabels;
            count = 4U;
            selected = static_cast<uint8_t>(timbres_module_);
            break;
        case AreaId::Ritmo:
            labels = kRitmoLabels;
            count = 4U;
            selected = static_cast<uint8_t>(ritmo_module_);
            break;
        case AreaId::Fx:
            labels = kFxLabels;
            count = 4U;
            selected = static_cast<uint8_t>(fx_module_);
            break;
        case AreaId::Mais:
            labels = kMaisLabels;
            count = 3U;
            selected = static_cast<uint8_t>(mais_module_);
            break;
        default:
            return;
    }

    for (uint8_t i = 0; i < count; ++i) {
        const int32_t y = 30 + static_cast<int32_t>(i) * 66;
        lv_obj_t* btn = createActionButton(sidebar_, labels[i], 6, y, 148, 58, nullptr, nullptr,
                                           i == selected, i == selected);
        sidebar_btns_[i] = btn;
        attachClick(btn, ControlTag::SidebarModule, static_cast<int32_t>(i));
    }

    const int32_t hdr_h = (header_variant_ == HeaderVariant::CompactOverlay) ? 44 : 68;
    const int32_t content_h = (kScreenHeight - 48) - hdr_h;
    if (content_h >= 340) {
        const char* foot_note = (active_area_ == AreaId::Fx)
                                    ? "CHORUS->DLY\n->REV->EQ FIX"
                                    : (active_area_ == AreaId::Ritmo) ? "ARP != SEQ\nESCOPO SEP." : "HIST. ATIVO";
        createWrapLabel(sidebar_, foot_note, 10, 296, 140, palette::kTextSecondary);
    }
}

void UiShell018A::rebuildActivePanel() {
    if (!panel_) return;
    lv_obj_clean(panel_);
    ++metrics_.module_mounts;

    tracked_params_.fill(TrackedParamWidget{});
    tracked_param_count_ = 0U;
    inspector_title_ = nullptr;
    inspector_values_ = nullptr;
    inspector_ack_ = nullptr;
    inspector_minus_btn_ = nullptr;
    inspector_plus_btn_ = nullptr;
    tocar_arp_pill_ = nullptr;
    tocar_seq_pill_ = nullptr;
    tocar_fx_pill_ = nullptr;
    tocar_harm_pill_ = nullptr;
    tocar_vkbd_ = nullptr;
    tocar_vkbd_info_ = nullptr;
    dynamic_feedback_label_ = nullptr;
    delay_status_label_ = nullptr;
    diag_live_label_ = nullptr;

    switch (active_area_) {
        case AreaId::Tocar:   buildTocarPanel(); break;
        case AreaId::Timbres: buildTimbresPanel(); break;
        case AreaId::Ritmo:   buildRitmoPanel(); break;
        case AreaId::Fx:      buildFxPanel(); break;
        case AreaId::Mais:    buildMaisPanel(); break;
        default: break;
    }
    refreshDynamicLabels();
    if (root_) lv_obj_update_layout(root_);
}

lv_obj_t* UiShell018A::addSectionCard(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h,
                                      const char* title, const char* badge, uint32_t badge_color) {
    lv_obj_t* card = lv_obj_create(parent);
    if (!card) return nullptr;
    applyPanelStyle(card, false);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    lv_obj_set_style_pad_all(card, 4, 0);
    if (title && title[0]) {
        createStyledLabel(card, title, 8, 5, w - (badge ? 124 : 16), &lv_font_montserrat_14, palette::kAccentPrimary);
    }
    if (badge && badge[0]) {
        createBadgePill(card, badge, w - 118, 3, 106, 22, badge_color, palette::kTextPrimary);
    }
    return card;
}

void UiShell018A::addParamSliderCard(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h,
                                     const char* id, const char* custom_title, bool force_readonly,
                                     const char* readonly_reason) {
    const ParameterDescriptor* desc = ParameterRegistry::findById(id);
    if (!desc) return;
    lv_obj_t* card = addSectionCard(parent, x, y, w, h, nullptr);
    if (!card) return;

    const char* title = custom_title ? custom_title : desc->name;
    createStyledLabel(card, title, 6, 4, w - 134, &lv_font_montserrat_14, palette::kTextPrimary);
    lv_obj_t* val_lbl = createStyledLabel(card, "--", w - 126, 4, 118, &lv_font_montserrat_14,
                                          palette::kAccentPrimary, LV_TEXT_ALIGN_RIGHT);
    lv_obj_t* st_lbl = createStyledLabel(card, "...", 6, 22, w - 16, &lv_font_montserrat_14, palette::kTextSecondary);

    const bool editable = !force_readonly && backend_.isParameterEditable(id);
    const int32_t scale = paramScaleFactor(desc);
    const int32_t min_s = static_cast<int32_t>(std::lround(desc->minimum * static_cast<float>(scale)));
    const int32_t max_s = static_cast<int32_t>(std::lround(desc->maximum * static_cast<float>(scale)));
    ParameterValue cur{};
    backend_.model().getValue(id, cur);

    const int32_t minus_w = 42;
    const int32_t plus_w = 42;
    const int32_t btn_y = h - 40;
    lv_obj_t* minus_b = createActionButton(card, "-", 6, btn_y, minus_w, 34, nullptr, nullptr, false, false);
    lv_obj_t* plus_b = createActionButton(card, "+", w - plus_w - 8, btn_y, plus_w, 34, nullptr, nullptr, false, false);

    lv_obj_t* slider = lv_slider_create(card);
    lv_slider_set_range(slider, min_s, max_s);
    lv_slider_set_value(slider, std::clamp(paramToSliderRaw(*desc, cur), min_s, max_s), LV_ANIM_OFF);
    lv_obj_set_pos(slider, 6 + minus_w + 10, btn_y + 8);
    lv_obj_set_size(slider, std::max<int32_t>(48, w - minus_w - plus_w - 34), 18);
    lv_obj_add_style(slider, &theme().slider_main, LV_PART_MAIN);
    lv_obj_add_style(slider, &theme().slider_indicator, LV_PART_INDICATOR);
    lv_obj_add_style(slider, &theme().slider_knob, LV_PART_KNOB);
    lv_obj_set_ext_click_area(slider, 10);

    if (!editable) {
        lv_obj_add_state(slider, LV_STATE_DISABLED);
        lv_obj_remove_flag(slider, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_state(minus_b, LV_STATE_DISABLED);
        lv_obj_remove_flag(minus_b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_state(plus_b, LV_STATE_DISABLED);
        lv_obj_remove_flag(plus_b, LV_OBJ_FLAG_CLICKABLE);
        lv_label_set_text(st_lbl, readonly_reason ? readonly_reason : featureStatusLabel(backend_.parameterStatus(id)));
    } else {
        attachValueChanged(slider, desc->id);
        attachClick(minus_b, ControlTag::ParamFineMinus, 0, 0, desc->id);
        attachClick(plus_b, ControlTag::ParamFinePlus, 0, 0, desc->id);
    }

    if (tracked_param_count_ < kMaxTrackedParams) {
        TrackedParamWidget& tw = tracked_params_[tracked_param_count_++];
        tw.id = desc->id;
        tw.card = card;
        tw.slider = slider;
        tw.value_label = val_lbl;
        tw.status_label = st_lbl;
        tw.sync_locked_visual = force_readonly;
    }
}

void UiShell018A::addParamSelectorCard(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h,
                                       const char* id, const char* custom_title) {
    const ParameterDescriptor* desc = ParameterRegistry::findById(id);
    if (!desc) return;
    lv_obj_t* card = addSectionCard(parent, x, y, w, h, nullptr);
    if (!card) return;

    createStyledLabel(card, custom_title ? custom_title : desc->name, 6, 4, w - 12, &lv_font_montserrat_14, palette::kTextSecondary);
    lv_obj_t* st_lbl = createStyledLabel(card, "", 6, 20, w - 12, &lv_font_montserrat_14, palette::kAccentPrimary);

    const int32_t btn_y = h - 42;
    lv_obj_t* prev_b = createActionButton(card, "<", 6, btn_y, 44, 36, nullptr, nullptr, false, false);
    lv_obj_t* val_btn = createActionButton(card, "--", 56, btn_y, std::max<int32_t>(48, w - 112), 36, nullptr, nullptr, true, false);
    lv_obj_t* next_b = createActionButton(card, ">", w - 50, btn_y, 44, 36, nullptr, nullptr, false, false);
    lv_obj_t* val_lbl = val_btn ? lv_obj_get_child(val_btn, 0) : nullptr;

    if (backend_.isParameterEditable(id)) {
        attachClick(prev_b, ControlTag::ParamEnumPrev, 0, 0, desc->id);
        attachClick(next_b, ControlTag::ParamEnumNext, 0, 0, desc->id);
        attachClick(val_btn, ControlTag::ParamCardSelect, 0, 0, desc->id);
    } else {
        lv_obj_add_state(prev_b, LV_STATE_DISABLED);
        lv_obj_remove_flag(prev_b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_state(next_b, LV_STATE_DISABLED);
        lv_obj_remove_flag(next_b, LV_OBJ_FLAG_CLICKABLE);
    }

    if (tracked_param_count_ < kMaxTrackedParams) {
        TrackedParamWidget& tw = tracked_params_[tracked_param_count_++];
        tw.id = desc->id;
        tw.card = card;
        tw.value_label = val_lbl;
        tw.status_label = st_lbl;
    }
}

void UiShell018A::addParamToggleCard(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h,
                                     const char* id, const char* custom_title) {
    const ParameterDescriptor* desc = ParameterRegistry::findById(id);
    if (!desc) return;
    lv_obj_t* card = addSectionCard(parent, x, y, w, h, nullptr);
    if (!card) return;

    createStyledLabel(card, custom_title ? custom_title : desc->name, 6, 4, w - 12, &lv_font_montserrat_14, palette::kTextSecondary);
    lv_obj_t* btn = createActionButton(card, "OFF", 6, h - 42, w - 12, 36, nullptr, nullptr, false, false);
    lv_obj_t* val_lbl = btn ? lv_obj_get_child(btn, 0) : nullptr;
    lv_obj_t* st_lbl = createStyledLabel(card, "", 6, 22, w - 12, &lv_font_montserrat_14, palette::kAccentPrimary);

    if (backend_.isParameterEditable(id)) {
        attachClick(btn, ControlTag::ParamToggle, 0, 0, desc->id);
    } else {
        lv_obj_add_state(btn, LV_STATE_DISABLED);
        lv_obj_remove_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    }

    if (tracked_param_count_ < kMaxTrackedParams) {
        TrackedParamWidget& tw = tracked_params_[tracked_param_count_++];
        tw.id = desc->id;
        tw.card = card;
        tw.value_label = val_lbl;
        tw.status_label = st_lbl;
        tw.toggle_btn = btn;
        tw.toggle_label = val_lbl;
    }
}

void UiShell018A::addParamContextInspector(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h) {
    lv_obj_t* card = addSectionCard(parent, x, y, w, h, "EDITOR CONTEXTUAL / DETALHES DE CONFIRMACAO");
    if (!card) return;
    inspector_title_ = createStyledLabel(card, "Selecionado: --", 8, 24, w - 140, &lv_font_montserrat_14, palette::kTextPrimary);
    inspector_values_ = createStyledLabel(card, "Local/Confirmado: --", 8, 42, w - 140, &lv_font_montserrat_14, palette::kAccentPrimary);
    inspector_ack_ = createStyledLabel(card, "Status owner: --", 8, 60, w - 140, &lv_font_montserrat_14, palette::kTextSecondary);

    inspector_minus_btn_ = createActionButton(card, "- FINO", w - 128, 22, 58, 40, nullptr, nullptr, false, false);
    inspector_plus_btn_ = createActionButton(card, "+ FINO", w - 66, 22, 58, 40, nullptr, nullptr, false, false);
    attachClick(inspector_minus_btn_, ControlTag::ParamFineMinus, -1, 0, nullptr);
    attachClick(inspector_plus_btn_, ControlTag::ParamFinePlus, 1, 0, nullptr);
}

void UiShell018A::addUnsupportedCard(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h,
                                      const char* title, const char* badge, const char* detail) {
    lv_obj_t* card = addSectionCard(parent, x, y, w, h, title, badge, palette::kUnavailableBadge);
    if (!card) return;
    lv_obj_set_style_border_color(card, lv_color_hex(palette::kWarning), 0);
    createWrapLabel(card, detail, 8, 28, w - 20, palette::kWarning);
}

void UiShell018A::buildTocarPanel() {
    // Full 800px stage area: no sidebar. Fits safely within h=364 (Header B) and h=388 (Header A).
    lv_obj_t* top = addSectionCard(panel_, 8, 6, 784, 52, nullptr);
    const auto& patch = backend_.model().activePatch();
    const SceneView sv = backend_.sceneView();
    char pbuf[96];
    std::snprintf(pbuf, sizeof(pbuf), "PATCH #%03u %s (%s | %s)",
                  unsigned(patch.id), patch.name, synthFamilyName(patch.family), patch.bank_name);
    createStyledLabel(top, pbuf, 8, 4, 420, &lv_font_montserrat_14, palette::kAccentPrimary);

    const char* active_sc_lbl = (sv.recall.active < scene::kSlots && backend_.sceneSlot(sv.recall.active))
                                    ? backend_.sceneSlot(sv.recall.active)->config.name
                                    : "--";
    char sbuf[80];
    std::snprintf(sbuf, sizeof(sbuf), "CENA ATIVA: %s | %s",
                  active_sc_lbl, scenePhaseText(sv.recall.phase, nullptr));
    createStyledLabel(top, sbuf, 8, 24, 380, &lv_font_montserrat_14, palette::kTextPrimary);

    tocar_arp_pill_ = createBadgePill(top, "ARP OFF", 404, 8, 90, 34, palette::kUnavailableBadge);
    tocar_seq_pill_ = createBadgePill(top, "SEQ OFF", 498, 8, 90, 34, palette::kUnavailableBadge);
    tocar_fx_pill_ = createBadgePill(top, "FX LIVE", 592, 8, 90, 34, palette::kPanelHighlight);
    tocar_harm_pill_ = createBadgePill(top, "HARM OFF", 686, 8, 90, 34, palette::kUnavailableBadge);

    // Sub-mode switcher bar (touch height 38px)
    const char* mode_names[5] = {"MACROS 1-4", "MACROS 5-8", "CENAS 1-8", "TECLADO", "ENCADEAR [PLAN]"};
    for (uint8_t i = 0; i < 5; ++i) {
        const bool sel = (static_cast<uint8_t>(tocar_mode_) == i);
        const bool plan = (i == 4);
        lv_obj_t* btn = createActionButton(panel_, mode_names[i], 8 + i * 158, 62, 150, 36,
                                           nullptr, nullptr, sel, sel && !plan);
        attachClick(btn, ControlTag::TocarModeBtn, static_cast<int32_t>(i));
    }

    if (tocar_mode_ == TocarMode::Macros1To4) {
        buildTocarMacrosGrid(0U, panel_);
    } else if (tocar_mode_ == TocarMode::Macros5To8) {
        buildTocarMacrosGrid(4U, panel_);
    } else if (tocar_mode_ == TocarMode::ScenesQuick) {
        buildTocarScenesPanel(panel_);
    } else if (tocar_mode_ == TocarMode::VirtualKeys) {
        buildTocarVirtualKeysPanel(panel_);
    } else {
        buildTocarChainProposalPanel(panel_);
    }
}

void UiShell018A::buildTocarMacrosGrid(uint8_t base_macro, lv_obj_t* parent) {
    const auto& patch = backend_.model().activePatch();
    for (uint8_t i = 0; i < 4; ++i) {
        const uint8_t m_idx = static_cast<uint8_t>(base_macro + i);
        const int32_t x = 8 + (i % 2) * 394;
        const int32_t y = 104 + (i / 2) * 86;
        char pid[24];
        std::snprintf(pid, sizeof(pid), "perf.macro.%u", unsigned(m_idx + 1U));
        const ParameterDescriptor* d = ParameterRegistry::findById(pid);
        const char* stable_id = d ? d->id : "perf.macro.1";
        char title[40];
        const char* macro_name = (patch.macro_labels[m_idx] && patch.macro_labels[m_idx][0])
                                     ? patch.macro_labels[m_idx]
                                     : "MACRO";
        std::snprintf(title, sizeof(title), "M%u %s", unsigned(m_idx + 1U), macro_name);
        addParamSliderCard(parent, x, y, 390, 80, stable_id, title);
    }

    // Bottom quick 8-Scene recall strip (y=280..356, fits inside Variant B h=364!)
    lv_obj_t* qs = addSectionCard(parent, 8, 280, 784, 78, "ACESSO RAPIDO - CENAS 1..8 (TOQUE P/ RECALL / SEL)");
    for (uint8_t s = 0; s < scene::kSlots; ++s) {
        const scene::Slot* slot = backend_.sceneSlot(s);
        const SceneView sv = backend_.sceneView();
        const bool is_active = (sv.recall.active == s);
        const bool is_pending = (sv.recall.pending == s);
        const bool is_valid = (slot && slot->state == seq::SlotState::Valid);
        char sname[24];
        if (is_valid && slot->config.name[0]) {
            std::snprintf(sname, sizeof(sname), "C%u %.7s", unsigned(s + 1U), slot->config.name);
        } else {
            std::snprintf(sname, sizeof(sname), "C%u VAZIO", unsigned(s + 1U));
        }
        lv_obj_t* sb = createActionButton(qs, sname, 6 + s * 96, 30, 92, 40, nullptr, nullptr,
                                          is_active || is_pending, is_pending);
        attachClick(sb, ControlTag::SceneSlotBtn, static_cast<int32_t>(s));
    }
}

void UiShell018A::buildTocarScenesPanel(lv_obj_t* parent) {
    const SceneView sv = backend_.sceneView();
    for (uint8_t s = 0; s < scene::kSlots; ++s) {
        const scene::Slot* slot = backend_.sceneSlot(s);
        const int32_t x = 8 + (s % 4) * 198;
        const int32_t y = 104 + (s / 4) * 84;
        const bool sel = (selected_scene_slot_ == s);
        const bool is_act = (sv.recall.active == s);
        const bool is_pend = (sv.recall.pending == s);
        const char* ph_txt = is_pend ? "PENDING" : (is_act ? "ACTIVE" : scenePhaseText(scene::RecallPhase::Idle, slot));
        if (is_act && sv.recall.phase == scene::RecallPhase::PartialApply) ph_txt = "PARTIAL";
        if (sel && sv.recall.phase == scene::RecallPhase::Failed) ph_txt = "FAILED";

        lv_obj_t* card = addSectionCard(parent, x, y, 190, 78, nullptr, ph_txt, scenePhaseColor(ph_txt));
        if (sel) {
            lv_obj_set_style_border_color(card, lv_color_hex(palette::kAccentPrimary), 0);
            lv_obj_set_style_border_width(card, 2, 0);
        }
        char lbl[32];
        if (slot && slot->state == seq::SlotState::Valid) {
            std::snprintf(lbl, sizeof(lbl), "C%u: %.10s", unsigned(s + 1U), slot->config.name);
        } else {
            std::snprintf(lbl, sizeof(lbl), "C%u: (EMPTY)", unsigned(s + 1U));
        }
        lv_obj_t* b = createActionButton(card, lbl, 4, 28, 180, 42, nullptr, nullptr, is_act || sel, is_pend);
        attachClick(b, ControlTag::SceneSlotBtn, static_cast<int32_t>(s));
    }

    // Bottom scene actions + performance locks strip (y=274..358)
    lv_obj_t* bar = addSectionCard(parent, 8, 274, 784, 84, nullptr);
    const uint8_t locks = sessionLockMask();
    char lbuf[96];
    std::snprintf(lbuf, sizeof(lbuf), "CENA %u | NOTICE: %s",
                  unsigned(selected_scene_slot_ + 1U), sv.notice ? sv.notice : "--");
    createStyledLabel(bar, lbuf, 8, 4, 764, &lv_font_montserrat_14, palette::kTextSecondary);

    lv_obj_t* rec_btn = createActionButton(bar, "RECALL", 8, 32, 92, 42, nullptr, nullptr, true, true);
    lv_obj_t* mod_btn = createActionButton(bar, "SELETIVO...", 104, 32, 114, 42, nullptr, nullptr, false, false);
    lv_obj_t* sav_btn = createActionButton(bar, "SALVAR", 222, 32, 86, 42, nullptr, nullptr, false, false);
    lv_obj_t* del_btn = createActionButton(bar, "LIMPAR", 312, 32, 86, 42, nullptr, nullptr, false, false);
    attachClick(rec_btn, ControlTag::SceneRecallBtn, static_cast<int32_t>(selected_scene_slot_));
    attachClick(mod_btn, ControlTag::SceneModalBtn, static_cast<int32_t>(selected_scene_slot_));
    attachClick(sav_btn, ControlTag::SceneSaveBtn, static_cast<int32_t>(selected_scene_slot_));
    attachClick(del_btn, ControlTag::SceneClearBtn, static_cast<int32_t>(selected_scene_slot_));

    const char* lnames[4] = {"LK FX", "LK HARM", "LK ARP", "LK MAC"};
    const uint8_t lbits[4] = {perf_lock::Fx, perf_lock::Harmony, perf_lock::Arp, perf_lock::Macros};
    for (uint8_t li = 0; li < 4; ++li) {
        const bool on = (locks & lbits[li]) != 0U;
        lv_obj_t* lb = createActionButton(bar, lnames[li], 412 + li * 92, 32, 86, 42, nullptr, nullptr, on, false);
        attachClick(lb, ControlTag::PerfLockBtn, static_cast<int32_t>(lbits[li]));
    }
}

void UiShell018A::buildTocarVirtualKeysPanel(lv_obj_t* parent) {
    lv_obj_t* strip = addSectionCard(parent, 8, 104, 784, 50, nullptr);
    const int32_t oct = backend_.model().getInt("perf.octave_shift", 0);
    char obuf[48];
    std::snprintf(obuf, sizeof(obuf), "TECLADO TOUCH 2 OITAVAS | OITAVA: %+d", int(oct));
    tocar_vkbd_info_ = createStyledLabel(strip, obuf, 8, 14, 520, &lv_font_montserrat_14, palette::kAccentPrimary);

    lv_obj_t* odn = createActionButton(strip, "OCT -", 548, 6, 108, 36, nullptr, nullptr, false, false);
    lv_obj_t* oup = createActionButton(strip, "OCT +", 664, 6, 108, 36, nullptr, nullptr, false, false);
    attachClick(odn, ControlTag::ParamFineMinus, -1, 0, "perf.octave_shift");
    attachClick(oup, ControlTag::ParamFinePlus, 1, 0, "perf.octave_shift");

    const int32_t kb_y = 160;
    const int32_t kb_h = 196;
    lv_obj_t* box = lv_obj_create(parent);
    applyPanelStyle(box, false);
    lv_obj_set_pos(box, 8, kb_y);
    lv_obj_set_size(box, 784, kb_h);
    lv_obj_set_style_pad_all(box, 2, 0);
    tocar_vkbd_ = box;

    auto* st = static_cast<ProtoKeyboardState*>(lv_malloc(sizeof(ProtoKeyboardState)));
    *st = ProtoKeyboardState{};
    st->backend = &backend_;
    st->box = box;
    st->info_lbl = tocar_vkbd_info_;

    lv_obj_add_event_cb(
        box,
        [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_DELETE) return;
            auto* s = static_cast<ProtoKeyboardState*>(lv_event_get_user_data(e));
            if (s) {
                releaseProtoKeyboardSlots(*s);
                for (uint8_t i = 0; i < s->key_count; ++i) {
                    if (s->keys[i].btn) lv_obj_remove_event_cb(s->keys[i].btn, onProtoKeyEvent);
                }
                lv_free(s);
            }
        },
        LV_EVENT_DELETE, st);

    const uint8_t num_octaves = 2;
    const uint8_t white_count = 15;
    const int32_t key_gap = 2;
    const int32_t white_w = (776 - key_gap * (white_count - 1)) / white_count;
    const int32_t white_h = kb_h - 8;
    const int32_t black_w = std::max<int32_t>(26, (white_w * 3) / 5);
    const int32_t black_h = (white_h * 58) / 100;

    for (uint8_t wk = 0; wk < white_count && st->key_count < 25U; ++wk) {
        const uint8_t oct_i = wk / 7U;
        const uint8_t deg = wk % 7U;
        const uint8_t semi = static_cast<uint8_t>(oct_i * 12U + kWhiteOffsets[deg]);
        const int32_t kx = 2 + wk * (white_w + key_gap);

        lv_obj_t* btn = lv_button_create(box);
        lv_obj_remove_style_all(btn);
        lv_obj_set_pos(btn, kx, 2);
        lv_obj_set_size(btn, white_w, white_h);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(palette::kBorder), 0);
        lv_obj_set_style_radius(btn, 3, 0);
        lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(btn, LV_OBJ_FLAG_PRESS_LOCK);

        char nbuf[8];
        std::snprintf(nbuf, sizeof(nbuf), "%s", kWhiteNames[deg]);
        lv_obj_t* kl = lv_label_create(btn);
        lv_label_set_text(kl, nbuf);
        lv_obj_set_style_text_font(kl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(kl, lv_color_hex(palette::kBackground), 0);
        lv_obj_align(kl, LV_ALIGN_BOTTOM_MID, 0, -4);

        const uint8_t slot_idx = st->key_count++;
        st->keys[slot_idx] = ProtoKeySlot{btn, semi, 60, 0, false, false};

        auto* kctx = static_cast<ProtoKeyEventCtx*>(lv_malloc(sizeof(ProtoKeyEventCtx)));
        kctx->owner = st;
        kctx->index = slot_idx;
        lv_obj_add_event_cb(btn, onProtoKeyEvent, LV_EVENT_ALL, kctx);
        lv_obj_add_event_cb(btn, onControlDeleted, LV_EVENT_DELETE, kctx);
    }

    for (uint8_t o = 0; o < num_octaves; ++o) {
        for (uint8_t b = 0; b < 5 && st->key_count < 25U; ++b) {
            const uint8_t wk = static_cast<uint8_t>(o * 7U + kBlackAfterWhite[b]);
            const uint8_t semi = static_cast<uint8_t>(o * 12U + kBlackSemis[b]);
            const int32_t boundary_x = 2 + (wk + 1) * (white_w + key_gap) - key_gap / 2;
            const int32_t bx = boundary_x - black_w / 2;

            lv_obj_t* btn = lv_button_create(box);
            lv_obj_remove_style_all(btn);
            lv_obj_set_pos(btn, bx, 2);
            lv_obj_set_size(btn, black_w, black_h);
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(btn, 1, 0);
            lv_obj_set_style_border_color(btn, lv_color_hex(palette::kBorder), 0);
            lv_obj_set_style_radius(btn, 3, 0);
            lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_remove_flag(btn, LV_OBJ_FLAG_PRESS_LOCK);

            const uint8_t slot_idx = st->key_count++;
            st->keys[slot_idx] = ProtoKeySlot{btn, semi, 60, 0, false, true};

            auto* kctx = static_cast<ProtoKeyEventCtx*>(lv_malloc(sizeof(ProtoKeyEventCtx)));
            kctx->owner = st;
            kctx->index = slot_idx;
            lv_obj_add_event_cb(btn, onProtoKeyEvent, LV_EVENT_ALL, kctx);
            lv_obj_add_event_cb(btn, onControlDeleted, LV_EVENT_DELETE, kctx);
        }
    }

    refreshProtoKeyboardColors(*st);
}

void UiShell018A::buildTocarChainProposalPanel(lv_obj_t* parent) {
    lv_obj_t* card = addSectionCard(parent, 8, 106, 784, 248,
                                    "ENCADEAMENTO DE CENAS - PROPOSTA UX 0.18-A",
                                    "NAO IMPL.", palette::kWarning);
    lv_obj_set_style_border_color(card, lv_color_hex(palette::kWarning), 0);
    createStyledLabel(card,
                      "MOCKUP VISUAL SOMENTE - NAO IMPLEMENTADO NO AUDIO OWNER / NVS DA 0.17B / 0.18-A.",
                      12, 32, 756, &lv_font_montserrat_14, palette::kWarning);

    const char* steps[4] = {"1: CENA 1 [4 COMP.]", "2: CENA 3 [8 COMP.]", "3: CENA 2 [2 COMP.]", "4: LOOP / STOP"};
    for (int i = 0; i < 4; ++i) {
        lv_obj_t* step_box = addSectionCard(card, 12 + i * 190, 58, 182, 86, nullptr);
        createStyledLabel(step_box, steps[i], 8, 8, 166, &lv_font_montserrat_14, palette::kTextPrimary);
        createStyledLabel(step_box, i == 0 ? "ATUAL (MOCK)" : "FILA (MOCK)", 8, 32, 166, &lv_font_montserrat_14, palette::kTextSecondary);
        createStyledLabel(step_box, "BOUNDARY: NEXT BAR", 8, 54, 166, &lv_font_montserrat_14, palette::kPlannedBadge);
    }

    createWrapLabel(card,
                    "Contrato futuro sugerido: ChainEngine no Audio Owner, disparo por fronteira de compasso, "
                    "desarme manual imediato e zero bloqueio no Panic ou troca manual de Cena.",
                    12, 156, 756, palette::kTextSecondary);
}

void UiShell018A::buildTimbresPanel() {
    switch (timbres_module_) {
        case TimbresModule::Biblioteca: buildTimbresBiblioteca(); break;
        case TimbresModule::Sintese:    buildTimbresSintese(); break;
        case TimbresModule::Camadas:    buildTimbresCamadas(); break;
        case TimbresModule::Modulacao:  buildTimbresModulacao(); break;
        default: break;
    }
}

void UiShell018A::buildTimbresBiblioteca() {
    const auto& active = backend_.model().activePatch();
    lv_obj_t* head = addSectionCard(panel_, 8, 6, 624, 62, "TIMBRES > BIBLIOTECA DE PATCHES",
                                    backend_.isPatchLoading() ? "CARREGANDO" : "PRONTO",
                                    backend_.hasPatchError() ? palette::kErrorPanic : palette::kActive);
    char cur[80];
    std::snprintf(cur, sizeof(cur), "#%03u %s  |  %s / %s",
                  unsigned(active.id), active.name, synthFamilyName(active.family), active.bank_name);
    createStyledLabel(head, cur, 8, 28, 380, &lv_font_montserrat_14, palette::kTextPrimary);

    lv_obj_t* prev_p = createActionButton(head, "< ANT", 412, 20, 96, 36, nullptr, nullptr, false, false);
    lv_obj_t* next_p = createActionButton(head, "PROX >", 516, 20, 98, 36, nullptr, nullptr, false, true);
    attachClick(prev_p, ControlTag::PatchBankSelectBtn, -1, 1);
    attachClick(next_p, ControlTag::PatchBankSelectBtn, 1, 1);

    size_t count = 0;
    const auto* entries = UiModel::catalog(count);
    lv_obj_t* grid = addSectionCard(panel_, 8, 74, 624, 210, "PATCHES DISPONIVEIS / FILTRADOS");
    uint8_t shown = 0;
    const size_t start_idx = (patch_bank_page_ * 6U < count) ? (patch_bank_page_ * 6U) : 0U;
    for (size_t i = start_idx; i < count && shown < 6U; ++i) {
        const auto& e = entries[i];
        const bool sel = (e.id == active.id);
        const bool selectable = backend_.isPatchSelectable(e.id);
        const int32_t bx = 8 + (shown % 2U) * 306;
        const int32_t by = 28 + (shown / 2U) * 58;
        char lbl[44];
        std::snprintf(lbl, sizeof(lbl), "#%03u %.14s [%s]",
                      unsigned(e.id), e.name, selectable ? synthFamilyName(e.family) : "N/A");
        lv_obj_t* pb = createActionButton(grid, lbl, bx, by, 298, 48, nullptr, nullptr, sel, sel);
        if (!selectable) {
            lv_obj_add_state(pb, LV_STATE_DISABLED);
            lv_obj_remove_flag(pb, LV_OBJ_FLAG_CLICKABLE);
        } else {
            attachClick(pb, ControlTag::PatchBankSelectBtn, static_cast<int32_t>(e.id), 0);
        }
        ++shown;
    }

    lv_obj_t* foot = addSectionCard(panel_, 8, 290, 624, 68, nullptr);
    lv_obj_t* pg_prev = createActionButton(foot, "< PAG", 8, 14, 96, 40, nullptr, nullptr, false, false);
    lv_obj_t* pg_next = createActionButton(foot, "PAG >", 110, 14, 96, 40, nullptr, nullptr, false, false);
    attachClick(pg_prev, ControlTag::PatchBankSelectBtn, -2, 2);
    attachClick(pg_next, ControlTag::PatchBankSelectBtn, 2, 2);

    if (backend_.patchNeedsDiscardConfirmation() && pending_discard_patch_ != 0xFFFFU) {
        char mbuf[64];
        std::snprintf(mbuf, sizeof(mbuf), "ALTERACOES NAO SALVAS -> CONFIRMAR #%03u?", unsigned(pending_discard_patch_));
        createStyledLabel(foot, mbuf, 214, 6, 280, &lv_font_montserrat_14, palette::kWarning);
        lv_obj_t* conf = createActionButton(foot, "CONFIRMAR", 498, 14, 118, 40, nullptr, nullptr, true, true);
        attachClick(conf, ControlTag::PatchDiscardConfirmBtn, static_cast<int32_t>(pending_discard_patch_));
    } else {
        const auto res = backend_.patchLoadResult();
        char fbuf[80];
        std::snprintf(fbuf, sizeof(fbuf), "BANCO PAG %u | %s",
                      unsigned(patch_bank_page_ + 1U), res.notice ? res.notice : featureStatusLabel(active.status));
        createStyledLabel(foot, fbuf, 214, 22, 400, &lv_font_montserrat_14, palette::kTextSecondary);
    }
}

void UiShell018A::buildTimbresSintese() {
    const auto& active = backend_.model().activePatch();
    const bool is_fm = (active.family == SynthFamily::FM);
    lv_obj_t* top = addSectionCard(panel_, 8, 6, 624, 72, "TIMBRES > SINTESE CONTEXTUAL",
                                   is_fm ? "MOTOR FM" : "MOTOR SUB", palette::kPanelHighlight);
    char sbuf[80];
    std::snprintf(sbuf, sizeof(sbuf), "Patch: #%03u %s | %s",
                  unsigned(active.id), active.name, backend_.soundEditStatus());
    createStyledLabel(top, sbuf, 8, 24, 608, &lv_font_montserrat_14, palette::kTextSecondary);

    const char* tab_names[4] = {"GERAL", is_fm ? "FM ALGO/OP" : "OSC SUB", "FILTRO / ENV", "ROUTING"};
    for (uint8_t t = 0; t < 4; ++t) {
        const bool sel = (static_cast<uint8_t>(synth_sub_tab_) == t);
        lv_obj_t* tb = createActionButton(panel_, tab_names[t], 8 + t * 158, 84, 150, 36, nullptr, nullptr, sel, sel);
        attachClick(tb, ControlTag::SynthSubTabBtn, static_cast<int32_t>(t));
    }

    if (synth_sub_tab_ == SynthSubTab::Geral) {
        addParamSliderCard(panel_, 8, 126, 306, 92, "amp.level");
        addParamSliderCard(panel_, 326, 126, 306, 92, "amp.pan");
        addParamSliderCard(panel_, 8, 224, 306, 92, "amp.vel_sens");
        addParamSliderCard(panel_, 326, 224, 306, 92, "amp.portamento_ms");
    } else if (synth_sub_tab_ == SynthSubTab::Motor) {
        if (is_fm) {
            addParamSelectorCard(panel_, 8, 126, 306, 92, "fm.algo");
            addParamSliderCard(panel_, 326, 126, 306, 92, "fm.feedback");
            addParamSliderCard(panel_, 8, 224, 306, 92, "fm.depth");
            addParamSliderCard(panel_, 326, 224, 306, 92, "fm.op.ratio");
        } else {
            addParamSelectorCard(panel_, 8, 126, 306, 92, "osc.1.wave");
            addParamSliderCard(panel_, 326, 126, 306, 92, "osc.1.detune_ct");
            addParamSliderCard(panel_, 8, 224, 306, 92, "osc.1.pulse_width");
            addParamSliderCard(panel_, 326, 224, 306, 92, "osc.sub_level");
        }
    } else if (synth_sub_tab_ == SynthSubTab::FiltroEnv) {
        addParamSliderCard(panel_, 8, 126, 306, 92, "filter.cutoff_hz");
        addParamSliderCard(panel_, 326, 126, 306, 92, "filter.resonance");
        addParamSliderCard(panel_, 8, 224, 306, 92, "env.amp.attack_ms");
        addParamSliderCard(panel_, 326, 224, 306, 92, "env.amp.release_ms");
    } else {
        addParamSelectorCard(panel_, 8, 126, 306, 92, "lfo.1.wave");
        addParamSliderCard(panel_, 326, 126, 306, 92, "lfo.1.rate_hz");
        addParamSliderCard(panel_, 8, 224, 306, 92, "lfo.1.depth");
        addParamSelectorCard(panel_, 326, 224, 306, 92, "lfo.1.dest");
    }
}

void UiShell018A::buildTimbresCamadas() {
    addSectionCard(panel_, 8, 6, 624, 96, "TIMBRES > CAMADAS (LAYER A / LAYER B)", "PLANNED", palette::kPlannedBadge);
    addUnsupportedCard(panel_, 8, 108, 624, 240,
                       "DUAL LAYER / SPLIT - NAO OPERACIONAL NA 0.17B", "PLANNED",
                       "Engine atual usa motor unificado de 8 vozes sem Layer B independente.\n"
                       "Os controles de Layer B / Split permanecem sinalizados como PLANNED e\n"
                       "nao simulam roteamento de audio inexistente.");
}

void UiShell018A::buildTimbresModulacao() {
    IUiBackend::RoutingSnapshot rs{};
    const bool has_routes = backend_.routingSnapshot(rs);
    lv_obj_t* top = addSectionCard(panel_, 8, 6, 624, 62, "TIMBRES > MODULACAO E MACROS",
                                   has_routes ? "ROUTING" : "DEMO",
                                   has_routes ? palette::kActive : palette::kWarning);
    char rbuf[96];
    if (has_routes) {
        std::snprintf(rbuf, sizeof(rbuf), "Epoch=%u Gen=%u Compare=%s Patch=#%03u",
                      unsigned(rs.epoch), unsigned(rs.generation), rs.compare ? "ON" : "OFF", unsigned(rs.patch));
    } else {
        std::snprintf(rbuf, sizeof(rbuf), "Matriz de modulacao completa: %s (use M1..M8 do Performance)",
                      featureStatusLabel(backend_.parameterStatus("mod.slot1.enable")));
    }
    createStyledLabel(top, rbuf, 8, 28, 608, &lv_font_montserrat_14, palette::kTextSecondary);

    addParamSliderCard(panel_, 8, 74, 306, 92, "perf.macro.1", "M1 (Route 1)");
    addParamSliderCard(panel_, 326, 74, 306, 92, "perf.macro.2", "M2 (Route 2)");
    addParamSliderCard(panel_, 8, 172, 306, 92, "perf.macro.3", "M3 (Route 3)");
    addParamSliderCard(panel_, 326, 172, 306, 92, "perf.macro.4", "M4 (Route 4)");
    addParamContextInspector(panel_, 8, 270, 624, 88);
}

void UiShell018A::buildRitmoPanel() {
    switch (ritmo_module_) {
        case RitmoModule::Sequenciador: buildRitmoSequencer(); break;
        case RitmoModule::Arpejador:    buildRitmoArpeggiator(); break;
        case RitmoModule::Padroes:      buildRitmoPatterns(); break;
        case RitmoModule::Bateria:      buildRitmoDrums(); break;
        default: break;
    }
}

void UiShell018A::buildRitmoSequencer() {
    const seq::Status* st = backend_.sequencerStatus();
    const seq::Pattern* pat = backend_.sequencerPattern();
    const bool live_seq = (st != nullptr && pat != nullptr);
    const bool playing = live_seq ? st->playing : backend_.model().getBool("seq.playing", false);
    const uint8_t cur_step = live_seq ? st->current_step : backend_.activeSequencerStep();
    const uint8_t pat_len = live_seq ? st->pattern_length : static_cast<uint8_t>(backend_.model().getInt("seq.length", 16));
    const unsigned div_idx = live_seq ? static_cast<unsigned>(st->division) % 8U : 5U;
    const uint8_t swing = live_seq ? st->swing : 50U;
    const int8_t transpose = live_seq ? st->transpose : static_cast<int8_t>(backend_.model().getInt("seq.transpose", 0));
    const bool seq_quant = live_seq ? st->quantize : false;
    const bool live_tr = live_seq ? st->midi.live : false;

    lv_obj_t* hdr = addSectionCard(panel_, 8, 6, 624, 62,
                                   live_seq ? "RITMO > SEQUENCIADOR (STEP 1..16)" : "RITMO > SEQUENCIADOR (DEMO RAM)",
                                   live_seq ? (playing ? "PLAYING" : "STOPPED") : "DEMO",
                                   playing ? palette::kActive : palette::kPlannedBadge);

    lv_obj_t* tr_btn = createActionButton(hdr, playing ? "STOP SEQ" : "PLAY SEQ", 8, 22, 128, 36, nullptr, nullptr, playing, playing);
    lv_obj_t* play_v = createActionButton(hdr, "PLAY MODE", 148, 22, 118, 36, nullptr, nullptr,
                                          seq_view_mode_ == SeqViewMode::Play, seq_view_mode_ == SeqViewMode::Play);
    lv_obj_t* edit_v = createActionButton(hdr, "EDIT MODE", 272, 22, 118, 36, nullptr, nullptr,
                                          seq_view_mode_ == SeqViewMode::Edit, seq_view_mode_ == SeqViewMode::Edit);
    attachClick(tr_btn, ControlTag::SeqTransportBtn);
    attachClick(play_v, ControlTag::SeqViewModeBtn, 0);
    attachClick(edit_v, ControlTag::SeqViewModeBtn, 1);

    char step_buf[16], slot_buf[16];
    if (cur_step < seq::kSteps) {
        std::snprintf(step_buf, sizeof(step_buf), "%u", unsigned(cur_step + 1U));
    } else {
        std::snprintf(step_buf, sizeof(step_buf), "--");
    }
    if (live_seq && st->active_slot < seq::kPatternSlots) {
        std::snprintf(slot_buf, sizeof(slot_buf), "%u", unsigned(st->active_slot + 1U));
    } else {
        std::snprintf(slot_buf, sizeof(slot_buf), "RAM");
    }
    char info[96];
    std::snprintf(info, sizeof(info), "PASSO:%s/%u | SLOT:%s", step_buf, unsigned(pat_len), slot_buf);
    createStyledLabel(hdr, info, 398, 28, 220, &lv_font_montserrat_14, palette::kAccentPrimary, LV_TEXT_ALIGN_RIGHT);

    if (seq_view_mode_ == SeqViewMode::Play) {
        // Clear visual separation of PLAY view: transport, step monitor bar, Scale Quantize vs Header Q, Live Transpose
        lv_obj_t* strip = addSectionCard(panel_, 8, 74, 624, 78, "GRADE ATIVA (MODO PLAY - SOMENTE EXECUCAO)");
        for (uint8_t i = 0; i < seq::kSteps; ++i) {
            const bool active = live_seq ? pat->steps[i].enabled : backend_.model().seqStep(i).active;
            const bool playhead = (i == cur_step);
            const int32_t sx = 8 + i * 38;
            lv_obj_t* cell = createBadgePill(strip, active ? "*" : "-", sx, 30, 34, 38,
                                             playhead ? palette::kAccentPrimary : (active ? palette::kActive : palette::kUnavailableBadge),
                                             playhead ? palette::kBackground : palette::kTextPrimary);
            (void)cell;
        }

        lv_obj_t* pcard = addSectionCard(panel_, 8, 158, 306, 128, "PARAMETROS DE EXECUCAO");
        char pb[96];
        std::snprintf(pb, sizeof(pb), "Div: %s | Swing: %u%% | Tr: %+d st",
                      kDivNames[div_idx], unsigned(swing), int(transpose));
        createStyledLabel(pcard, pb, 8, 28, 286, &lv_font_montserrat_14, palette::kTextPrimary);
        lv_obj_t* db = createActionButton(pcard, "DIV >", 8, 60, 90, 40, nullptr, nullptr, false, false);
        lv_obj_t* td = createActionButton(pcard, "TR -", 104, 60, 92, 40, nullptr, nullptr, false, false);
        lv_obj_t* tu = createActionButton(pcard, "TR +", 202, 60, 92, 40, nullptr, nullptr, false, false);
        attachClick(db, ControlTag::SeqDivisionBtn);
        attachClick(td, ControlTag::SeqStepFieldBtn, 10, -1);
        attachClick(tu, ControlTag::SeqStepFieldBtn, 10, 1);

        lv_obj_t* hcard = addSectionCard(panel_, 326, 158, 306, 128, "HARMONIA SEQ != Q DO HEADER");
        createWrapLabel(hcard, "SEQ QUANT usa Root/Scale (indep. de Q)", 8, 26, 286, palette::kTextSecondary);
        lv_obj_t* sq = createActionButton(hcard, seq_quant ? "SEQ QUANT: ON" : "SEQ QUANT: OFF",
                                          8, 64, 142, 40, nullptr, nullptr, seq_quant, seq_quant);
        lv_obj_t* lt = createActionButton(hcard, live_tr ? "LIVE TR: ON" : "LIVE TR: OFF",
                                          156, 64, 142, 40, nullptr, nullptr, live_tr, false);
        attachClick(sq, ControlTag::SeqScaleQuantBtn);
        attachClick(lt, ControlTag::SeqLiveTransposeBtn);

        lv_obj_t* info_b = addSectionCard(panel_, 8, 292, 624, 66, nullptr);
        std::snprintf(info, sizeof(info), "Notas do SEQ: NoteOrigin::Sequencer (CH forcado OFF) | Status: %s",
                      live_seq ? (st->modified ? "MODIFICADO" : "LIMPO") : "UI DEMO");
        dynamic_feedback_label_ = createStyledLabel(info_b, info, 8, 12, 602, &lv_font_montserrat_14, palette::kTextSecondary);
        if (dynamic_feedback_label_) lv_label_set_long_mode(dynamic_feedback_label_, LV_LABEL_LONG_WRAP);
    } else {
        // EDIT MODE: 16 selectable step pads + per-step edit controls
        lv_obj_t* grid = addSectionCard(panel_, 8, 74, 624, 122, "MODO EDIT - TOQUE NO PASSO 1..16 PARA SELECIONAR");
        for (uint8_t i = 0; i < seq::kSteps; ++i) {
            const bool active = live_seq ? pat->steps[i].enabled : backend_.model().seqStep(i).active;
            const bool sel = (i == selected_seq_step_);
            const int32_t sx = 8 + (i % 8) * 76;
            const int32_t sy = 26 + (i / 8) * 44;
            char sl[16];
            std::snprintf(sl, sizeof(sl), "%u%s", unsigned(i + 1U), active ? "*" : "");
            lv_obj_t* sb = createActionButton(grid, sl, sx, sy, 70, 38, nullptr, nullptr, active, sel);
            attachClick(sb, ControlTag::SeqStepSelectBtn, static_cast<int32_t>(i));
        }

        const seq::Step step_data = live_seq
                                        ? pat->steps[selected_seq_step_]
                                        : seq::Step{backend_.model().seqStep(selected_seq_step_).note,
                                                    backend_.model().seqStep(selected_seq_step_).velocity, 50,
                                                    backend_.model().seqStep(selected_seq_step_).active};
        char nbuf[12];
        formatNoteName(step_data.note, nbuf, sizeof(nbuf));
        lv_obj_t* ed = addSectionCard(panel_, 8, 202, 624, 156, "EDICAO DO PASSO SELECIONADO");
        char sb_txt[96];
        std::snprintf(sb_txt, sizeof(sb_txt), "PASSO %u | ESTADO: %s | NOTA: %s (%u) | VEL: %u | GATE: %u%%",
                      unsigned(selected_seq_step_ + 1U), step_data.enabled ? "ON" : "REST",
                      nbuf, unsigned(step_data.note), unsigned(step_data.velocity), unsigned(step_data.gate));
        createStyledLabel(ed, sb_txt, 8, 26, 608, &lv_font_montserrat_14, palette::kAccentPrimary);

        lv_obj_t* tgl = createActionButton(ed, step_data.enabled ? "PASSO: ON" : "PASSO: REST", 8, 56, 134, 42, nullptr, nullptr, step_data.enabled, step_data.enabled);
        lv_obj_t* nm  = createActionButton(ed, "NOTA -", 148, 56, 86, 42, nullptr, nullptr, false, false);
        lv_obj_t* np  = createActionButton(ed, "NOTA +", 238, 56, 86, 42, nullptr, nullptr, false, false);
        lv_obj_t* vm  = createActionButton(ed, "VEL -", 330, 56, 72, 42, nullptr, nullptr, false, false);
        lv_obj_t* vp  = createActionButton(ed, "VEL +", 406, 56, 72, 42, nullptr, nullptr, false, false);
        const bool step_rec = live_seq && (st->midi.mode == seq::InputMode::StepRecord);
        lv_obj_t* sr  = createActionButton(ed, step_rec ? "STEP REC ON" : "STEP REC", 484, 56, 132, 42, nullptr, nullptr, step_rec, step_rec);
        attachClick(tgl, ControlTag::SeqStepToggleBtn, static_cast<int32_t>(selected_seq_step_));
        attachClick(nm, ControlTag::SeqStepFieldBtn, 0, -1);
        attachClick(np, ControlTag::SeqStepFieldBtn, 0, 1);
        attachClick(vm, ControlTag::SeqStepFieldBtn, 1, -10);
        attachClick(vp, ControlTag::SeqStepFieldBtn, 1, 10);
        attachClick(sr, ControlTag::SeqStepRecBtn);

        createStyledLabel(ed, "Alternar para PLAY MODE preserva transporte e notas ativas sem reset do AMY.",
                          8, 112, 602, &lv_font_montserrat_14, palette::kTextSecondary);
    }
}

void UiShell018A::buildRitmoArpeggiator() {
    const arp::Status* ast = backend_.arpStatus();
    const bool enabled = ast ? ast->config.enabled : backend_.model().getBool("arp.enable", false);
    const bool armed = ast ? ast->armed : enabled;
    const bool latch = ast ? ast->config.latch : backend_.model().getBool("arp.latch", false);

    lv_obj_t* hdr = addSectionCard(panel_, 8, 6, 624, 62,
                                   ast ? "RITMO > ARPEJADOR (TRANSPORTE INDEPENDENTE DO SEQ)"
                                       : "RITMO > ARPEJADOR (DEMO RAM - SEM OWNER)",
                                   armed ? "ARMED" : (enabled ? "ARP ON" : "ARP OFF"),
                                   armed ? palette::kActive : (enabled ? palette::kAccentPrimary : palette::kPlannedBadge));
    lv_obj_t* en_b   = createActionButton(hdr, enabled ? "ARP: ON" : "ARP: OFF", 8, 22, 84, 36, nullptr, nullptr, enabled, enabled);
    lv_obj_t* play_b = createActionButton(hdr, "PLAY ARP", 96, 22, 84, 36, nullptr, nullptr, armed, armed);
    lv_obj_t* stop_b = createActionButton(hdr, "STOP ARP", 184, 22, 84, 36, nullptr, nullptr, false, false);
    lv_obj_t* clr_b  = createActionButton(hdr, "CLR LATCH", 272, 22, 88, 36, nullptr, nullptr, latch, false);
    attachClick(en_b, ControlTag::ParamToggle, 0, 0, "arp.enable");
    attachClick(play_b, ControlTag::ArpTransportBtn, 1);
    attachClick(stop_b, ControlTag::ArpTransportBtn, 0);
    attachClick(clr_b, ControlTag::ArpLatchClearBtn);

    char abuf[64];
    std::snprintf(abuf, sizeof(abuf), "H:%u P:%u | %s",
                  ast ? unsigned(ast->held_count) : 0U,
                  ast ? unsigned(ast->length) : 0U,
                  backend_.arpNotice() ? backend_.arpNotice() : "OK");
    createStyledLabel(hdr, abuf, 368, 28, 248, &lv_font_montserrat_14, palette::kTextSecondary, LV_TEXT_ALIGN_RIGHT);

    addParamSelectorCard(panel_, 8, 74, 306, 92, "arp.direction", "Direcao");
    addParamSelectorCard(panel_, 326, 74, 306, 92, "arp.rate", "Taxa / Divisao");
    addParamSliderCard(panel_, 8, 172, 306, 92, "arp.octaves", "Oitavas");
    addParamSliderCard(panel_, 326, 172, 306, 92, "arp.gate_pct", "Gate %");
    addParamToggleCard(panel_, 8, 270, 306, 86, "arp.latch", "Latch");
    addParamSliderCard(panel_, 326, 270, 306, 86, "arp.swing_pct", "Swing %");
}

void UiShell018A::buildRitmoPatterns() {
    lv_obj_t* top = addSectionCard(panel_, 8, 6, 624, 54, "RITMO > BIBLIOTECA DE PADROES (SLOTS 1..8)",
                                   backend_.patternBusy() ? "BUSY" : (backend_.isDemoMode() ? "DEMO" : "NVS"),
                                   backend_.isDemoMode() ? palette::kWarning : palette::kActive);
    const char* ntxt = backend_.patternNotice();
    createStyledLabel(top, (ntxt && ntxt[0]) ? ntxt : "Selecione um slot para Load / Save / Delete.",
                      8, 26, 608, &lv_font_montserrat_14, palette::kTextSecondary);

    lv_obj_t* grid = addSectionCard(panel_, 8, 66, 624, 218, "SLOTS");
    for (uint8_t s = 0; s < 8; ++s) {
        const seq::Slot* sl = backend_.patternSlot(s);
        const bool sel = (selected_pattern_slot_ == s);
        const bool valid = (sl && sl->state == seq::SlotState::Valid);
        char lbl[32];
        if (valid) {
            std::snprintf(lbl, sizeof(lbl), "P%u %.11s", unsigned(s + 1U), sl->name);
        } else {
            std::snprintf(lbl, sizeof(lbl), "P%u (VAZIO)", unsigned(s + 1U));
        }
        lv_obj_t* b = createActionButton(grid, lbl, 8 + (s % 2U) * 306, 26 + (s / 2U) * 46, 298, 40,
                                         nullptr, nullptr, valid, sel);
        attachClick(b, ControlTag::PatternSlotBtn, static_cast<int32_t>(s));
    }

    lv_obj_t* act = addSectionCard(panel_, 8, 290, 624, 68, nullptr);
    lv_obj_t* lb = createActionButton(act, "LOAD SLOT", 8, 14, 180, 40, nullptr, nullptr, false, true);
    lv_obj_t* sb = createActionButton(act, "SAVE SLOT", 218, 14, 180, 40, nullptr, nullptr, false, false);
    lv_obj_t* db = createActionButton(act, "DELETE SLOT", 428, 14, 188, 40, nullptr, nullptr, false, false);
    attachClick(lb, ControlTag::PatternLoadBtn);
    attachClick(sb, ControlTag::PatternSaveBtn);
    attachClick(db, ControlTag::PatternDeleteBtn);
}

void UiShell018A::buildRitmoDrums() {
    addSectionCard(panel_, 8, 6, 624, 62, "RITMO > BATERIA / DRUMS", "PLANNED", palette::kPlannedBadge);
    addUnsupportedCard(panel_, 8, 76, 624, 276,
                       "DRUM ENGINE - NAO OPERACIONAL NA 0.17B / 0.18-A", "PLANNED",
                       "Os parametros drums.* sao PLANNED no ParameterRegistry.\n"
                       "Nenhuma faixa de bateria toca ou dispara notas reais no runtime AMY atual.\n"
                       "Nenhum handler simulado foi conectado para evitar controle enganoso.");
}

void UiShell018A::buildFxPanel() {
    switch (fx_module_) {
        case FxModule::Delay:  buildFxDelay(); break;
        case FxModule::Reverb: buildFxReverb(); break;
        case FxModule::Chorus: buildFxChorus(); break;
        case FxModule::Master: buildFxMaster(); break;
        default: break;
    }
}

void UiShell018A::buildFxDelay() {
    IUiBackend::DelayStatus ds{};
    const bool has_ds = backend_.delayStatus(ds);
    const bool sync_on = (backend_.model().getInt("fx.delay.mode", 0) == 1) || (has_ds && ds.sync);

    lv_obj_t* top = addSectionCard(panel_, 8, 6, 624, 58, "FX > DELAY (CADEIA FIXA: CHORUS -> DELAY -> REVERB -> EQ)",
                                   sync_on ? "SYNC ON" : "FREE MS",
                                   sync_on ? palette::kAccentPrimary : palette::kActive);
    delay_status_label_ = createStyledLabel(top, "", 8, 26, 608, &lv_font_montserrat_14, palette::kTextSecondary);

    addParamSelectorCard(panel_, 8, 70, 306, 92, "fx.delay.mode", "Modo (Free / Sync)");
    addParamSelectorCard(panel_, 326, 70, 306, 92, "fx.delay.division", "Divisao Sync");
    addParamSliderCard(panel_, 8, 168, 306, 92, "fx.delay.time_ms",
                       sync_on ? "Tempo (SYNC LOCK)" : "Tempo (ms)",
                       sync_on, "SYNC LOCKED");
    addParamSliderCard(panel_, 326, 168, 306, 92, "fx.delay.feedback", "Feedback %");
    addParamSliderCard(panel_, 8, 266, 306, 92, "fx.delay.mix", "Mix % (Manual)");

    addUnsupportedCard(panel_, 326, 266, 306, 92,
                       "BYPASS INDIV.", "BYPASS N/A",
                       "Sem bypass indiv. em FxConfig/IUiBackend.\nMix=0% NAO corta M3/M7.");
}

void UiShell018A::buildFxReverb() {
    addSectionCard(panel_, 8, 6, 624, 54, "FX > REVERB (CADEIA FIXA P4)",
                   featureStatusLabel(backend_.parameterStatus("fx.reverb.mix")), palette::kActive);

    addParamSliderCard(panel_, 8, 68, 306, 108, "fx.reverb.size", "Tamanho / Decay %");
    addParamSliderCard(panel_, 326, 68, 306, 108, "fx.reverb.mix", "Mix % (Manual)");

    addUnsupportedCard(panel_, 8, 184, 624, 86,
                       "BYPASS INDIVIDUAL DE REVERB - INDISPONIVEL", "BYPASS N/A",
                       "Nao ha comando ON/OFF ou bypass de Reverb em FxConfig/IUiBackend. "
                       "Reduzir Mix para 0% NAO equivale a bypass nem neutraliza macros M6/M7.");

    addParamContextInspector(panel_, 8, 276, 624, 82);
}

void UiShell018A::buildFxChorus() {
    const int32_t mode = backend_.model().getInt("fx.chorus.mode", 0);
    addSectionCard(panel_, 8, 6, 624, 54, "FX > CHORUS (MODO OFF REAL NO AUDIO OWNER)",
                   mode == 0 ? "CHORUS OFF" : "ATIVO",
                   mode == 0 ? palette::kPlannedBadge : palette::kActive);

    addParamSelectorCard(panel_, 8, 68, 306, 108, "fx.chorus.mode", "Modo Chorus (incl. OFF real)");
    addParamSliderCard(panel_, 326, 68, 306, 108, "fx.chorus.depth", "Profundidade %",
                       mode == 0, "CHORUS OFF");

    lv_obj_t* info = addSectionCard(panel_, 8, 184, 624, 84, "COMPORTAMENTO CONFIRMADO (fx_state.h / amy_runtime.cpp)");
    createWrapLabel(info,
                    "Selecionar OFF em fx.chorus.mode aplica level=0 no runtime AMY (unico FX com OFF real).\n"
                    "Demais modos: CLASSIC, JUNO I, JUNO II, ENSEMBLE, VIBRATO.",
                    8, 24, 608, palette::kTextPrimary);

    addParamContextInspector(panel_, 8, 274, 624, 84);
}

void UiShell018A::buildFxMaster() {
    addSectionCard(panel_, 8, 6, 624, 54, "FX > MASTER & EQ (CADEIA FIXA P4)",
                   "PARCIAL", palette::kAccentPrimary);

    addParamSliderCard(panel_, 8, 68, 306, 92, "perf.master_volume", "Volume Master (MIDI 0..127)");
    addParamSliderCard(panel_, 326, 68, 306, 92, "fx.master_tone", "Tone Master (-100..+100%)");
    addParamSliderCard(panel_, 8, 166, 306, 92, "fx.eq.low_db", "EQ Low (-12..+12 dB)");

    addUnsupportedCard(panel_, 326, 166, 306, 92,
                       "FX.DRIVE / FX.ROUTING", "PLANNED",
                       "Drive e Routing flexivel estao PLANNED e indisponiveis em IUiBackend.");

    addUnsupportedCard(panel_, 8, 264, 624, 94,
                       "GAPS DE CONTRATO IDENTIFICADOS PARA 0.18-B (SEM BYPASS FALSO)", "GAP 0.18-B",
                       "FxConfig ja possui eq_mid_db, eq_high_db e bypass_all no Audio Owner,\n"
                       "mas NAO possuem ParameterDescriptor/handler em IUiBackend na 0.17B.\n"
                       "Painel omite sliders falsos ate extensao de contrato na 0.18-B.");
}

void UiShell018A::buildMaisPanel() {
    switch (mais_module_) {
        case MaisModule::MidiSmk25:    buildMaisMidi(); break;
        case MaisModule::Preferencias: buildMaisPreferences(); break;
        case MaisModule::Diagnosticos: buildMaisDiagnostics(); break;
        default: break;
    }
}

void UiShell018A::buildMaisMidi() {
    lv_obj_t* top = addSectionCard(panel_, 8, 6, 624, 82, "MAIS > MIDI / M-VAVE SMK25 V2",
                                   backend_.isConnected() ? "CONECTADO" : "SEM USB",
                                   backend_.isConnected() ? palette::kActive : palette::kWarning);
    char mbuf[120];
    std::snprintf(mbuf, sizeof(mbuf), "Perfil: %s | Banco: %s | Learn: %s",
                  backend_.midiProfileName()[0] ? backend_.midiProfileName() : "(Demo)",
                  backend_.midiBankName()[0] ? backend_.midiBankName() : "--",
                  backend_.midiLearnStatus()[0] ? backend_.midiLearnStatus() : "Idle");
    createStyledLabel(top, mbuf, 8, 28, 608, &lv_font_montserrat_14, palette::kTextPrimary);

    addParamSliderCard(panel_, 8, 94, 306, 92, "harmony.usb_channel", "Canal USB p/ Harmonia (1..16)");
    addParamSelectorCard(panel_, 326, 94, 306, 92, "midi.vel_curve", "Curva Velocidade");

    lv_obj_t* actions = addSectionCard(panel_, 8, 192, 624, 166, "ACOES DO PERFIL USB MIDI");
    lv_obj_t* bn = createActionButton(actions, "PROX. BANCO KNOBS", 8, 30, 196, 42, nullptr, nullptr, false, true);
    lv_obj_t* lr = createActionButton(actions, "LEARN", 214, 30, 120, 42, nullptr, nullptr, false, false);
    lv_obj_t* lm = createActionButton(actions, "LEARN MOD WHEEL", 344, 30, 180, 42, nullptr, nullptr, false, false);
    lv_obj_t* rl = createActionButton(actions, "RELOAD", 532, 30, 84, 42, nullptr, nullptr, false, false);
    attachClick(bn, ControlTag::MidiActionBtn, static_cast<int32_t>(IUiBackend::MidiAction::NextBank));
    attachClick(lr, ControlTag::MidiActionBtn, static_cast<int32_t>(IUiBackend::MidiAction::Learn));
    attachClick(lm, ControlTag::MidiActionBtn, static_cast<int32_t>(IUiBackend::MidiAction::LearnModWheel));
    attachClick(rl, ControlTag::MidiActionBtn, static_cast<int32_t>(IUiBackend::MidiAction::ReloadProfile));

    createStyledLabel(actions,
                      "Nota sobre escopo: USB Direct e Arpeggiator so recebem Quantizer(Q)/Chord(CH)\n"
                      "quando o canal MIDI coincide exatamente com harmony.usb_channel (sem Omni).",
                      8, 84, 608, &lv_font_montserrat_14, palette::kTextSecondary);
}

void UiShell018A::buildMaisPreferences() {
    addSectionCard(panel_, 8, 6, 624, 54, "MAIS > PREFERENCIAS DE SESSAO E LOCKS", "SESSAO", palette::kPanelHighlight);

    lv_obj_t* locks_card = addSectionCard(panel_, 8, 68, 624, 104, "PERFORMANCE LOCKS GLOBAIS (PRESERVA ENTRE PATCHES / CENAS)");
    const uint8_t locks = sessionLockMask();
    const char* lnames[4] = {"KEEP FX", "KEEP HARMONY", "KEEP ARP", "KEEP MACROS"};
    const uint8_t lbits[4] = {perf_lock::Fx, perf_lock::Harmony, perf_lock::Arp, perf_lock::Macros};
    for (uint8_t i = 0; i < 4; ++i) {
        const bool on = (locks & lbits[i]) != 0U;
        lv_obj_t* b = createActionButton(locks_card, lnames[i], 8 + i * 154, 34, 146, 48, nullptr, nullptr, on, on);
        attachClick(b, ControlTag::PerfLockBtn, static_cast<int32_t>(lbits[i]));
    }

    lv_obj_t* hdr_card = addSectionCard(panel_, 8, 178, 624, 90, "VARIANTE DE CABECALHO EM TESTE (0.18-A)");
    const bool is_a = (header_variant_ == HeaderVariant::CompactOverlay);
    lv_obj_t* ba = createActionButton(hdr_card, "VARIANTE A: COMPACTO (44px) + OVERLAYS", 8, 30, 300, 44, nullptr, nullptr, is_a, is_a);
    lv_obj_t* bb = createActionButton(hdr_card, "VARIANTE B: 2 LINHAS (68px)", 316, 30, 300, 44, nullptr, nullptr, !is_a, !is_a);
    attachClick(ba, ControlTag::HeaderVariantBtn, 0);
    attachClick(bb, ControlTag::HeaderVariantBtn, 1);

    lv_obj_t* tempo_c = addSectionCard(panel_, 8, 274, 624, 84, "TEMPO GLOBAL / NVS");
    lv_obj_t* st_btn = createActionButton(tempo_c, "ABRIR OVERLAY BPM", 8, 28, 180, 42, nullptr, nullptr, false, false);
    lv_obj_t* sv_btn = createActionButton(tempo_c, "SALVAR BPM P/ NVS", 196, 28, 180, 42, nullptr, nullptr, false, backend_.canSaveTempo());
    attachClick(st_btn, ControlTag::HeaderBpmBtn);
    attachClick(sv_btn, ControlTag::BpmSaveBtn);
    createStyledLabel(tempo_c, backend_.tempoSaveNotice()[0] ? backend_.tempoSaveNotice() : "--",
                      386, 36, 230, &lv_font_montserrat_14, palette::kTextSecondary);
}

void UiShell018A::buildMaisDiagnostics() {
    addSectionCard(panel_, 8, 6, 624, 54, "MAIS > DIAGNOSTICOS DO PROTOTIPO E BACKEND",
                   backend_.isDemoMode() ? "DEMO N/OP" : "ADAPTER 017B",
                   backend_.isDemoMode() ? palette::kWarning : palette::kActive);

    lv_obj_t* box = addSectionCard(panel_, 8, 68, 624, 290, "TELEMETRIA HOST LVGL + LIMITES DE QUALIFICACAO");
    diag_live_label_ = createWrapLabel(box, "--", 8, 28, 608, palette::kTextPrimary);
    createWrapLabel(box,
                    "AVISO CRITICO: Testes e capturas em host NAO comprovam audio I2S,\n"
                    "touch GT911, PSRAM ou USB Host fisico na Guition ESP32-P4.\n"
                    "Pendencias fisicas da 0.17B seguem obrigatorias antes de qualquer merge.",
                    8, 200, 608, palette::kWarning);
}

void UiShell018A::rebuildOverlay() {
    if (overlay_backdrop_) {
        lv_obj_delete(overlay_backdrop_);
        overlay_backdrop_ = nullptr;
        overlay_card_ = nullptr;
    }
    overlay_status_label_ = nullptr;
    overlay_q_btn_ = nullptr;
    overlay_ch_btn_ = nullptr;
    if (active_overlay_ == OverlayKind::None) return;

    const int32_t hdr_h = (header_variant_ == HeaderVariant::CompactOverlay) ? 44 : 68;
    const int32_t content_h = (kScreenHeight - 48) - hdr_h;

    // Cover ONLY content area between header and footer so header PANIC & footer stay accessible!
    overlay_backdrop_ = lv_obj_create(root_);
    lv_obj_remove_style_all(overlay_backdrop_);
    lv_obj_set_pos(overlay_backdrop_, 0, hdr_h);
    lv_obj_set_size(overlay_backdrop_, kScreenWidth, content_h);
    lv_obj_set_style_bg_opa(overlay_backdrop_, LV_OPA_70, 0);
    lv_obj_set_style_bg_color(overlay_backdrop_, lv_color_hex(0x05090D), 0);
    lv_obj_remove_flag(overlay_backdrop_, LV_OBJ_FLAG_SCROLLABLE);

    const int32_t card_h = std::min<int32_t>(content_h - 16, 344);
    overlay_card_ = lv_obj_create(overlay_backdrop_);
    applyPanelStyle(overlay_card_, true);
    lv_obj_set_pos(overlay_card_, 24, 8);
    lv_obj_set_size(overlay_card_, kScreenWidth - 48, card_h);
    lv_obj_set_style_pad_all(overlay_card_, 6, 0);

    lv_obj_t* close_b = createActionButton(overlay_card_, "FECHAR", kScreenWidth - 48 - 118, 6, 108, 36,
                                           nullptr, nullptr, false, true);
    attachClick(close_b, ControlTag::CloseOverlayBtn);

    if (active_overlay_ == OverlayKind::TempoBpm) {
        createStyledLabel(overlay_card_, "OVERLAY > TEMPO GLOBAL (BPM) E TAP", 10, 12, 500,
                          &lv_font_montserrat_14, palette::kAccentPrimary);
        const int32_t bpm = backend_.model().getInt("perf.bpm", 120);
        char bbuf[48];
        std::snprintf(bbuf, sizeof(bbuf), "%d BPM", int(bpm));
        overlay_status_label_ = createStyledLabel(overlay_card_, bbuf, 16, 62, 220,
                                                  &lv_font_montserrat_28, palette::kTextPrimary);

        const int deltas[4] = {-5, -1, 1, 5};
        const char* dlbls[4] = {"-5 BPM", "-1 BPM", "+1 BPM", "+5 BPM"};
        for (int i = 0; i < 4; ++i) {
            lv_obj_t* db = createActionButton(overlay_card_, dlbls[i], 16 + i * 116, 122, 108, 46,
                                              nullptr, nullptr, false, false);
            attachClick(db, ControlTag::BpmDeltaBtn, deltas[i]);
        }
        lv_obj_t* tap_b = createActionButton(overlay_card_, "TAP TEMPO", 16, 186, 220, 48, nullptr, nullptr, true, true);
        lv_obj_t* sav_b = createActionButton(overlay_card_, "SALVAR NO NVS", 248, 186, 220, 48, nullptr, nullptr, false, backend_.canSaveTempo());
        attachClick(tap_b, ControlTag::BpmTapBtn);
        attachClick(sav_b, ControlTag::BpmSaveBtn);

        createStyledLabel(overlay_card_,
                          "PANIC no topo e navegacao no rodape permanecem visiveis e acessiveis acima deste overlay.",
                          16, 258, 700, &lv_font_montserrat_14, palette::kTextSecondary);
    } else if (active_overlay_ == OverlayKind::HarmonyContext) {
        createStyledLabel(overlay_card_, "OVERLAY HARMONICO > ESCALA (Q), ACORDE (CH) E ESCOPO", 10, 12, 580,
                          &lv_font_montserrat_14, palette::kAccentPrimary);

        const auto& hc = backend_.model().live_harmony.configuration;
        const bool q_on = backend_.model().getBool("harmony.scale_enable", hc.scale_on);
        const bool ch_on = backend_.model().getBool("harmony.chord_enable", hc.chord_on);
        const bool touch_on = backend_.model().getBool("harmony.apply_touch", hc.touch);
        const int32_t root_i = backend_.model().getInt("harmony.root_note", hc.root);
        const int32_t scale_i = backend_.model().getInt("harmony.scale_type", hc.scale);
        const int32_t quant_i = backend_.model().getInt("harmony.quant_mode", hc.quant);
        const int32_t chord_i = backend_.model().getInt("harmony.chord_type", hc.chord);
        const int32_t voic_i = backend_.model().getInt("harmony.voicing", hc.open);
        const int32_t usb_ch = backend_.model().getInt("harmony.usb_channel", hc.channel + 1);

        overlay_q_btn_ = createActionButton(overlay_card_, q_on ? "Q QUANT: ON" : "Q QUANT: OFF", 12, 46, 168, 40, nullptr, nullptr, q_on, q_on);
        overlay_ch_btn_ = createActionButton(overlay_card_, ch_on ? "CH ACORDE: ON" : "CH ACORDE: OFF", 188, 46, 178, 40, nullptr, nullptr, ch_on, ch_on);
        lv_obj_t* touch_b = createActionButton(overlay_card_, touch_on ? "TOUCH HARM: ON" : "TOUCH HARM: OFF",
                                               374, 46, 178, 40, nullptr, nullptr, touch_on, false);
        lv_obj_t* preset_b = createActionButton(overlay_card_, "PRESET C MAJ", 560, 46, 172, 40, nullptr, nullptr, false, false);
        attachClick(overlay_q_btn_, ControlTag::OverlayQBtn);
        attachClick(overlay_ch_btn_, ControlTag::OverlayChBtn);
        attachClick(touch_b, ControlTag::OverlayHarmApplyTouchBtn);
        attachClick(preset_b, ControlTag::OverlayHarmPresetBtn, 1);

        const ParameterDescriptor* d_root = ParameterRegistry::findById("harmony.root_note");
        const ParameterDescriptor* d_scale = ParameterRegistry::findById("harmony.scale_type");
        const ParameterDescriptor* d_quant = ParameterRegistry::findById("harmony.quant_mode");
        const ParameterDescriptor* d_chord = ParameterRegistry::findById("harmony.chord_type");
        const ParameterDescriptor* d_voic = ParameterRegistry::findById("harmony.voicing");

        struct EnumRow { const char* id; const char* lbl; int32_t val; const ParameterDescriptor* desc; };
        const EnumRow rows[5] = {
            {"harmony.root_note", "Fundamental", root_i, d_root},
            {"harmony.scale_type", "Escala", scale_i, d_scale},
            {"harmony.quant_mode", "Modo Quant.", quant_i, d_quant},
            {"harmony.chord_type", "Tipo Acorde", chord_i, d_chord},
            {"harmony.voicing", "Voicing", voic_i, d_voic}
        };
        for (int r = 0; r < 5; ++r) {
            const int32_t rx = 12 + (r % 2) * 368;
            const int32_t ry = 94 + (r / 2) * 44;
            const char* vtxt = (rows[r].desc && rows[r].desc->enum_values &&
                                rows[r].val >= 0 && rows[r].val < rows[r].desc->enum_count)
                                   ? rows[r].desc->enum_values[rows[r].val]
                                   : "--";
            char tbuf[48];
            std::snprintf(tbuf, sizeof(tbuf), "%s: %s", rows[r].lbl, vtxt);
            lv_obj_t* p = createActionButton(overlay_card_, "<", rx, ry, 42, 38, nullptr, nullptr, false, false);
            lv_obj_t* n = createActionButton(overlay_card_, ">", rx + 314, ry, 42, 38, nullptr, nullptr, false, false);
            createBadgePill(overlay_card_, tbuf, rx + 46, ry, 264, 38, palette::kPanel);
            attachClick(p, ControlTag::ParamEnumPrev, 0, 0, rows[r].id);
            attachClick(n, ControlTag::ParamEnumNext, 0, 0, rows[r].id);
        }

        // Honest scope explanation box + rejected feedback readout
        char sbuf[256];
        std::snprintf(sbuf, sizeof(sbuf),
                      "ESCOPO: Q e CH atuam em USB Canal %d + Arp + Touch(%s). "
                      "SEQ NAO usa Q/CH (usa SEQ SCALE QUANTIZE e forca CH=OFF).\n"
                      "ADD9/9TH e DROP2/SPREAD/OCT+5 nao sao suportados e sao rejeitados pelo owner.",
                      int(usb_ch), touch_on ? "ON" : "OFF");
        overlay_status_label_ = createWrapLabel(overlay_card_, sbuf, 12, 228, 720, palette::kTextSecondary);
    } else if (active_overlay_ == OverlayKind::SceneSelectiveModal) {
        createStyledLabel(overlay_card_, "MODAL > RECALL / SALVAR SELETIVO DE CENA", 10, 12, 540,
                          &lv_font_montserrat_14, palette::kAccentPrimary);
        const char* dnames[5] = {"PATCH", "PADRAO SEQ", "FX", "HARMONIA", "ARP"};
        const uint8_t dbits[5] = {scene::Patch, scene::Pattern, scene::Fx, scene::Harmony, scene::Arp};
        for (int i = 0; i < 5; ++i) {
            const bool on = (selective_capture_mask_ & dbits[i]) != 0U;
            lv_obj_t* db = createActionButton(overlay_card_, dnames[i], 14 + i * 144, 70, 136, 48, nullptr, nullptr, on, on);
            attachClick(db, ControlTag::SceneDomainBtn, static_cast<int32_t>(dbits[i]));
        }
        lv_obj_t* rec = createActionButton(overlay_card_, "APLICAR SELETIVO NA CENA ATIVA", 14, 148, 340, 48,
                                           nullptr, nullptr, true, true);
        attachClick(rec, ControlTag::SceneRecallBtn, static_cast<int32_t>(selected_scene_slot_), 1);

        char mbuf[80];
        std::snprintf(mbuf, sizeof(mbuf), "Slot alvo: CENA %u | Locks globais preservados conforme Preferencias.",
                      unsigned(selected_scene_slot_ + 1U));
        overlay_status_label_ = createStyledLabel(overlay_card_, mbuf, 14, 218, 700, &lv_font_montserrat_14, palette::kTextSecondary);
    }
    if (root_) lv_obj_update_layout(root_);
}

void UiShell018A::refreshDynamicLabels() {
    refreshHeaderDynamic();
    refreshParamWidgets();

    if (active_area_ == AreaId::Tocar) {
        const arp::Status* ast = backend_.arpStatus();
        const seq::Status* sst = backend_.sequencerStatus();
        const bool arp_on = ast ? ast->config.enabled : backend_.model().getBool("arp.enable", false);
        const bool seq_on = sst ? sst->playing : backend_.model().getBool("seq.playing", false);
        const bool q_on = backend_.model().getBool("harmony.scale_enable", false);
        const bool ch_on = backend_.model().getBool("harmony.chord_enable", false);

        if (tocar_arp_pill_) {
            lv_obj_set_style_bg_color(tocar_arp_pill_, lv_color_hex(arp_on ? palette::kActive : palette::kUnavailableBadge), 0);
            if (lv_obj_t* l = lv_obj_get_child(tocar_arp_pill_, 0)) lv_label_set_text(l, arp_on ? "ARP ON" : "ARP OFF");
        }
        if (tocar_seq_pill_) {
            lv_obj_set_style_bg_color(tocar_seq_pill_, lv_color_hex(seq_on ? palette::kActive : palette::kUnavailableBadge), 0);
            if (lv_obj_t* l = lv_obj_get_child(tocar_seq_pill_, 0)) {
                char sb[16];
                if (seq_on && sst && sst->current_step < seq::kSteps) {
                    std::snprintf(sb, sizeof(sb), "SEQ S%u", unsigned(sst->current_step + 1U));
                } else {
                    std::snprintf(sb, sizeof(sb), seq_on ? "SEQ ON" : "SEQ OFF");
                }
                lv_label_set_text(l, sb);
            }
        }
        if (tocar_fx_pill_) {
            lv_obj_set_style_bg_color(tocar_fx_pill_,
                                      lv_color_hex(backend_.isDemoMode() ? palette::kWarning : palette::kPanelHighlight), 0);
            if (lv_obj_t* l = lv_obj_get_child(tocar_fx_pill_, 0)) {
                lv_label_set_text_static(l, backend_.isDemoMode() ? "FX DEMO" : "FX LIVE");
                lv_obj_invalidate(l);
            }
        }
        if (tocar_harm_pill_) {
            const bool hon = q_on || ch_on;
            lv_obj_set_style_bg_color(tocar_harm_pill_, lv_color_hex(hon ? palette::kAccentPrimary : palette::kUnavailableBadge), 0);
            if (lv_obj_t* l = lv_obj_get_child(tocar_harm_pill_, 0)) {
                char hb[16];
                std::snprintf(hb, sizeof(hb), "%s/%s", q_on ? "Q" : "-", ch_on ? "CH" : "-");
                lv_label_set_text(l, hb);
            }
        }
        if (tocar_vkbd_) {
            if (auto* st = static_cast<ProtoKeyboardState*>(lv_obj_get_user_data(tocar_vkbd_))) {
                refreshProtoKeyboardColors(*st);
            }
        }
    }

    if (active_area_ == AreaId::Fx && fx_module_ == FxModule::Delay && delay_status_label_) {
        IUiBackend::DelayStatus ds{};
        if (backend_.delayStatus(ds)) {
            char db[96];
            std::snprintf(db, sizeof(db), "Alvo: %.1f ms | Aplicado: %.1f ms | %s | Bypass=%s",
                          double(ds.target_ms), double(ds.applied_ms),
                          ds.out_of_range ? "CLIP" : "OK", ds.bypassed ? "ON" : "OFF");
            lv_label_set_text(delay_status_label_, db);
        } else {
            lv_label_set_text(delay_status_label_, "DEMO: status detalhado do buffer de delay indisponivel sem Audio Owner.");
        }
    }

    if (active_area_ == AreaId::Mais && mais_module_ == MaisModule::Diagnosticos && diag_live_label_) {
        const LvglHeapStats hs = readLvglHeapStats();
        const auto& d = backend_.model().diagnostics();
        char tbuf[320];
        std::snprintf(tbuf, sizeof(tbuf),
                      "Backend: %s (%s)\n"
                      "LVGL Heap: total=%uB | free=%uB | max_blk=%uB | peak_used=%uB | frag=%u%%\n"
                      "Objetos ativos: %u | Shell builds=%u | Module mounts=%u | Inplace=%u\n"
                      "Vozes=%u/%u | Rev=%u | Flash Notice=%s",
                      backend_.backendName(), backend_.statusText(),
                      unsigned(hs.total_size), unsigned(hs.free_bytes), unsigned(hs.free_biggest_size),
                      unsigned(hs.max_used), unsigned(hs.frag_pct),
                      unsigned(countAllObjects()), unsigned(metrics_.shell_builds),
                      unsigned(metrics_.module_mounts), unsigned(metrics_.inplace_updates),
                      unsigned(d.active_voices), unsigned(d.max_voices),
                      unsigned(backend_.model().revision()),
                      status_flash_text_ ? status_flash_text_ : "NONE");
        lv_label_set_text(diag_live_label_, tbuf);
    }

    if (inspector_title_ && inspector_values_ && inspector_ack_ && selected_param_id_) {
        const ParameterDescriptor* desc = ParameterRegistry::findById(selected_param_id_);
        ParameterValue cur{};
        if (desc && backend_.model().getValue(desc->id, cur)) {
            char vbuf[40];
            ParameterRegistry::formatValue(*desc, cur, vbuf, sizeof(vbuf));
            char tbuf[64], abuf[96];
            std::snprintf(tbuf, sizeof(tbuf), "Parametro: %s (%s)", desc->name, desc->id);
            std::snprintf(abuf, sizeof(abuf), "Valor confirmado: %s [%s]",
                          vbuf, (desc->unit && desc->unit[0]) ? desc->unit : "-");
            lv_label_set_text(inspector_title_, tbuf);
            lv_label_set_text(inspector_values_, abuf);
            lv_label_set_text(inspector_ack_, status_flash_text_ ? status_flash_text_
                                                                 : commandStateLabel(backend_.lastCommandState()));
        }
    }

    if (active_overlay_ == OverlayKind::HarmonyContext) {
        const auto& hc = backend_.model().live_harmony.configuration;
        const bool q_on = backend_.model().getBool("harmony.scale_enable", hc.scale_on);
        const bool ch_on = backend_.model().getBool("harmony.chord_enable", hc.chord_on);
        if (overlay_q_btn_) {
            applyButtonStyle(overlay_q_btn_, q_on, q_on);
            if (lv_obj_t* l = lv_obj_get_child(overlay_q_btn_, 0)) {
                lv_label_set_text(l, q_on ? "Q QUANT: ON" : "Q QUANT: OFF");
            }
        }
        if (overlay_ch_btn_) {
            applyButtonStyle(overlay_ch_btn_, ch_on, ch_on);
            if (lv_obj_t* l = lv_obj_get_child(overlay_ch_btn_, 0)) {
                lv_label_set_text(l, ch_on ? "CH ACORDE: ON" : "CH ACORDE: OFF");
            }
        }
    }
}

void UiShell018A::refreshHeaderDynamic() {
    if (!header_) return;
    const bool demo = backend_.isDemoMode();
    if (hdr_mode_label_) {
        lv_label_set_text_static(hdr_mode_label_, demo ? "DEMO N/OP" : "P4 LIVE");
        lv_obj_invalidate(hdr_mode_label_);
    }

    const auto& patch = backend_.model().activePatch();
    const SceneView sv = backend_.sceneView();
    if (hdr_patch_label_) {
        const char* fam = (patch.family == SynthFamily::FM) ? "FM" : "SUB";
        std::snprintf(hdr_patch_buf_, sizeof(hdr_patch_buf_), "#%03u %.15s [%s]",
                      unsigned(patch.id), patch.name, fam);
        lv_label_set_text_static(hdr_patch_label_, hdr_patch_buf_);
        lv_obj_invalidate(hdr_patch_label_);
    }
    if (hdr_scene_label_) {
        if (sv.recall.pending < scene::kSlots) {
            std::snprintf(hdr_scene_buf_, sizeof(hdr_scene_buf_), "SC>%u PEND", unsigned(sv.recall.pending + 1U));
        } else if (sv.recall.active < scene::kSlots) {
            std::snprintf(hdr_scene_buf_, sizeof(hdr_scene_buf_), "SC:%u %s", unsigned(sv.recall.active + 1U),
                          sv.recall.phase == scene::RecallPhase::PartialApply ? "PART" : "ACT");
        } else {
            std::snprintf(hdr_scene_buf_, sizeof(hdr_scene_buf_), "SC:-- %s", demo ? "RAM" : "LIVE");
        }
        lv_label_set_text_static(hdr_scene_label_, hdr_scene_buf_);
        lv_obj_invalidate(hdr_scene_label_);
    }

    const int32_t bpm = backend_.model().getInt("perf.bpm", 120);
    if (hdr_bpm_label_) {
        std::snprintf(hdr_bpm_buf_, sizeof(hdr_bpm_buf_), "%d BPM", int(bpm));
        lv_label_set_text_static(hdr_bpm_label_, hdr_bpm_buf_);
        lv_obj_invalidate(hdr_bpm_label_);
    }

    const int32_t root = std::clamp<int32_t>(backend_.model().getInt("harmony.root_note", 0), 0, 11);
    const int32_t scale = std::clamp<int32_t>(backend_.model().getInt("harmony.scale_type", 1), 0, 10);
    const auto& hstate = backend_.model().live_harmony;
    const bool q_on = backend_.model().getBool("harmony.scale_enable", hstate.configuration.scale_on);
    const bool ch_on = backend_.model().getBool("harmony.chord_enable", hstate.configuration.chord_on);

    if (hdr_harm_label_) {
        const ParameterDescriptor* dq = ParameterRegistry::findById("harmony.quant_mode");
        const int32_t qidx = std::clamp<int32_t>(backend_.model().getInt("harmony.quant_mode", 0), 0, 5);
        const char* qtxt = (dq && dq->enum_values && qidx < dq->enum_count) ? dq->enum_values[qidx] : "QUANT";
        if (header_variant_ == HeaderVariant::ExpandedTwoRow) {
            std::snprintf(hdr_harm_buf_, sizeof(hdr_harm_buf_), "%s %s [%s]", kRootNames[root], kScaleShort[scale], qtxt);
        } else {
            std::snprintf(hdr_harm_buf_, sizeof(hdr_harm_buf_), "%s %s", kRootNames[root], kScaleShort[scale]);
        }
        lv_label_set_text_static(hdr_harm_label_, hdr_harm_buf_);
        lv_obj_invalidate(hdr_harm_label_);
    }
    if (hdr_q_btn_ && hdr_q_label_) {
        const bool wait_q = hstate.pending && backend_.lastCommandParameterId() &&
                            std::strcmp(backend_.lastCommandParameterId(), "harmony.scale_enable") == 0;
        setPersistentButtonState(hdr_q_btn_, q_on, q_on || wait_q);
        lv_label_set_text_static(hdr_q_label_, wait_q ? "Q .." : (q_on ? "Q ON" : "Q OFF"));
        lv_obj_invalidate(hdr_q_label_);
    }
    if (hdr_ch_btn_ && hdr_ch_label_) {
        const bool wait_ch = hstate.pending && backend_.lastCommandParameterId() &&
                             std::strcmp(backend_.lastCommandParameterId(), "harmony.chord_enable") == 0;
        setPersistentButtonState(hdr_ch_btn_, ch_on, ch_on || wait_ch);
        lv_label_set_text_static(hdr_ch_label_, wait_ch ? "CH .." : (ch_on ? "CH ON" : "CH OFF"));
        lv_obj_invalidate(hdr_ch_label_);
    }
    if (hdr_row2_info_label_) {
        const int32_t usb_ch = backend_.model().getInt("harmony.usb_channel", 1);
        std::snprintf(hdr_row2_buf_, sizeof(hdr_row2_buf_), "USB Ch%d+Arp%s | %s",
                      int(usb_ch),
                      backend_.model().getBool("harmony.apply_touch", true) ? "+Touch" : "",
                      status_flash_text_ ? status_flash_text_ : backend_.statusText());
        lv_label_set_text_static(hdr_row2_info_label_, hdr_row2_buf_);
        lv_obj_invalidate(hdr_row2_info_label_);
    }
}

void UiShell018A::refreshParamWidgets() {
    for (uint8_t i = 0; i < tracked_param_count_; ++i) {
        TrackedParamWidget& tw = tracked_params_[i];
        if (!tw.id) continue;
        const ParameterDescriptor* desc = ParameterRegistry::findById(tw.id);
        if (!desc) continue;
        ParameterValue pv{};
        if (!backend_.model().getValue(tw.id, pv)) continue;

        char vbuf[40];
        ParameterRegistry::formatValue(*desc, pv, vbuf, sizeof(vbuf));
        if (tw.value_label) {
            lv_label_set_text(tw.value_label, vbuf);
        }
        if (tw.slider && !tw.sync_locked_visual) {
            const int32_t target = paramToSliderRaw(*desc, pv);
            if (lv_slider_get_value(tw.slider) != target) {
                lv_slider_set_value(tw.slider, target, LV_ANIM_OFF);
            }
        }
        if (tw.toggle_btn) {
            applyButtonStyle(tw.toggle_btn, pv.asBool(), pv.asBool());
        }
        if (tw.status_label && !tw.sync_locked_visual) {
            const FeatureStatus fs = backend_.parameterStatus(tw.id);
            const char* st = featureStatusLabel(fs);
            if (backend_.lastCommandParameterId() && std::strcmp(backend_.lastCommandParameterId(), tw.id) == 0) {
                st = commandStateLabel(backend_.lastCommandState());
            } else if (fs == FeatureStatus::Available && !backend_.isDemoMode()) {
                st = "CONFIRMED";
            }
            lv_label_set_text(tw.status_label, st);
        }
    }
}

void UiShell018A::handleBindingAction(const ControlBinding& b) {
    switch (b.tag) {
        case ControlTag::FooterArea:
            selectArea(static_cast<AreaId>(b.arg0));
            return;
        case ControlTag::SidebarModule:
            if (active_area_ == AreaId::Timbres) selectTimbresModule(static_cast<TimbresModule>(b.arg0));
            else if (active_area_ == AreaId::Ritmo) selectRitmoModule(static_cast<RitmoModule>(b.arg0));
            else if (active_area_ == AreaId::Fx) selectFxModule(static_cast<FxModule>(b.arg0));
            else if (active_area_ == AreaId::Mais) selectMaisModule(static_cast<MaisModule>(b.arg0));
            return;
        case ControlTag::TocarModeBtn:
            selectTocarMode(static_cast<TocarMode>(b.arg0));
            return;
        case ControlTag::HeaderBpmBtn:
            openOverlay(active_overlay_ == OverlayKind::TempoBpm ? OverlayKind::None : OverlayKind::TempoBpm);
            return;
        case ControlTag::HeaderHarmBtn:
            openOverlay(active_overlay_ == OverlayKind::HarmonyContext ? OverlayKind::None : OverlayKind::HarmonyContext);
            return;
        case ControlTag::HeaderQBtn:
        case ControlTag::OverlayQBtn: {
            const bool cur = backend_.model().getBool("harmony.scale_enable", false);
            const auto res = backend_.requestParameterBool("harmony.scale_enable", !cur);
            status_flash_text_ = res.notice ? res.notice : commandStateLabel(res.state);
            refreshDynamicLabels();
            return;
        }
        case ControlTag::HeaderChBtn:
        case ControlTag::OverlayChBtn: {
            const bool cur = backend_.model().getBool("harmony.chord_enable", false);
            const auto res = backend_.requestParameterBool("harmony.chord_enable", !cur);
            status_flash_text_ = res.notice ? res.notice : commandStateLabel(res.state);
            refreshDynamicLabels();
            return;
        }
        case ControlTag::HeaderPanicBtn: {
            const auto res = backend_.requestPanic();
            status_flash_text_ = res.notice ? res.notice : "PANIC SENT";
            refreshDynamicLabels();
            return;
        }
        case ControlTag::HeaderVariantBtn:
            if (b.arg0 == 0 && b.arg1 == 0 && active_area_ != AreaId::Mais) {
                setHeaderVariant(header_variant_ == HeaderVariant::CompactOverlay ? HeaderVariant::ExpandedTwoRow
                                                                                  : HeaderVariant::CompactOverlay);
            } else {
                setHeaderVariant(b.arg0 == 1 ? HeaderVariant::ExpandedTwoRow : HeaderVariant::CompactOverlay);
            }
            return;
        case ControlTag::CloseOverlayBtn:
            closeOverlay();
            return;
        case ControlTag::ParamCardSelect:
            if (b.param_id) {
                const ParameterDescriptor* d = ParameterRegistry::findById(b.param_id);
                if (d && d->type == ParameterValueType::Enum && d->enum_count > 0U) {
                    ParameterValue pv{};
                    backend_.model().getValue(b.param_id, pv);
                    const int32_t next = (pv.asInt() + 1) % d->enum_count;
                    const auto res = backend_.requestParameterValue(b.param_id, ParameterValue::makeEnum(next));
                    status_flash_text_ = res.notice ? res.notice : commandStateLabel(res.state);
                    if (active_area_ == AreaId::Fx || active_overlay_ != OverlayKind::None) {
                        if (active_overlay_ != OverlayKind::None) rebuildOverlay();
                        else rebuildActivePanel();
                    }
                    refreshDynamicLabels();
                } else {
                    selectParameter(b.param_id);
                }
            }
            return;
        case ControlTag::ParamFineMinus:
            if (b.param_id) selected_param_id_ = b.param_id;
            stepSelectedParameter(-1);
            if (active_area_ == AreaId::Tocar && tocar_mode_ == TocarMode::VirtualKeys && tocar_vkbd_) {
                if (tocar_vkbd_info_) {
                    char obuf[48];
                    std::snprintf(obuf, sizeof(obuf), "TECLADO TOUCH 2 OITAVAS | OITAVA: %+d",
                                  int(backend_.model().getInt("perf.octave_shift", 0)));
                    lv_label_set_text(tocar_vkbd_info_, obuf);
                }
            }
            return;
        case ControlTag::ParamFinePlus:
            if (b.param_id) selected_param_id_ = b.param_id;
            stepSelectedParameter(1);
            if (active_area_ == AreaId::Tocar && tocar_mode_ == TocarMode::VirtualKeys && tocar_vkbd_) {
                if (tocar_vkbd_info_) {
                    char obuf[48];
                    std::snprintf(obuf, sizeof(obuf), "TECLADO TOUCH 2 OITAVAS | OITAVA: %+d",
                                  int(backend_.model().getInt("perf.octave_shift", 0)));
                    lv_label_set_text(tocar_vkbd_info_, obuf);
                }
            }
            return;
        case ControlTag::ParamToggle:
            if (b.param_id) {
                selected_param_id_ = b.param_id;
                const bool cur = backend_.model().getBool(b.param_id, false);
                const auto res = backend_.requestParameterBool(b.param_id, !cur);
                status_flash_text_ = res.notice ? res.notice : commandStateLabel(res.state);
                if (std::strcmp(b.param_id, "arp.enable") == 0) {
                    rebuildActivePanel();
                } else {
                    refreshDynamicLabels();
                }
            }
            return;
        case ControlTag::ParamEnumPrev:
        case ControlTag::ParamEnumNext:
            if (b.param_id) {
                selected_param_id_ = b.param_id;
                const ParameterDescriptor* d = ParameterRegistry::findById(b.param_id);
                if (d && d->enum_count > 0U) {
                    ParameterValue pv{};
                    backend_.model().getValue(b.param_id, pv);
                    const int32_t delta = (b.tag == ControlTag::ParamEnumNext) ? 1 : -1;
                    int32_t next = (pv.asInt() + delta + d->enum_count) % d->enum_count;
                    const auto res = backend_.requestParameterValue(b.param_id, ParameterValue::makeEnum(next));
                    status_flash_text_ = res.notice ? res.notice : commandStateLabel(res.state);
                    if (active_overlay_ != OverlayKind::None) rebuildOverlay();
                    else if (active_area_ == AreaId::Fx) rebuildActivePanel();
                    refreshDynamicLabels();
                }
            }
            return;
        case ControlTag::SeqViewModeBtn:
            selectSeqViewMode(b.arg0 == 1 ? SeqViewMode::Edit : SeqViewMode::Play);
            return;
        case ControlTag::SeqTransportBtn: {
            if (const seq::Status* st = backend_.sequencerStatus()) {
                seq::Request req{};
                req.action = st->playing ? seq::Action::Stop : seq::Action::Play;
                const auto res = backend_.requestSequencer(req);
                status_flash_text_ = res.notice ? res.notice : commandStateLabel(res.state);
            } else {
                const bool p = backend_.model().getBool("seq.playing", false);
                const auto res = backend_.requestParameterBool("seq.playing", !p);
                status_flash_text_ = res.notice ? res.notice : commandStateLabel(res.state);
            }
            rebuildActivePanel();
            return;
        }
        case ControlTag::SeqStepSelectBtn:
            selectSeqStep(static_cast<uint8_t>(b.arg0));
            return;
        case ControlTag::SeqStepToggleBtn: {
            const uint8_t s = static_cast<uint8_t>(b.arg0);
            if (backend_.sequencerStatus()) {
                seq::Request req{};
                req.action = seq::Action::ToggleStepEnabled;
                req.index = s;
                const auto res = backend_.requestSequencer(req);
                status_flash_text_ = res.notice ? res.notice : commandStateLabel(res.state);
            } else {
                backend_.model().toggleSeqStep(s);
                status_flash_text_ = "SEQ DEMO RAM STEP";
            }
            rebuildActivePanel();
            return;
        }
        case ControlTag::SeqStepFieldBtn: {
            if (const seq::Status* st = backend_.sequencerStatus()) {
                const seq::Pattern* pat = backend_.sequencerPattern();
                seq::Request req{};
                if (b.arg0 == 10) {
                    req.action = seq::Action::SetTranspose;
                    req.value = static_cast<int16_t>(std::clamp<int>(int(st->transpose) + b.arg1, -24, 24));
                } else if (b.arg0 == 0 && pat) {
                    req.action = seq::Action::SetStepNote;
                    req.index = selected_seq_step_;
                    req.value = static_cast<int16_t>(std::clamp<int>(int(pat->steps[selected_seq_step_].note) + b.arg1, 0, 127));
                } else if (b.arg0 == 1 && pat) {
                    req.action = seq::Action::SetStepVelocity;
                    req.index = selected_seq_step_;
                    req.value = static_cast<int16_t>(std::clamp<int>(int(pat->steps[selected_seq_step_].velocity) + b.arg1, 1, 127));
                }
                const auto res = backend_.requestSequencer(req);
                status_flash_text_ = res.notice ? res.notice : commandStateLabel(res.state);
            }
            rebuildActivePanel();
            return;
        }
        case ControlTag::SeqScaleQuantBtn:
            if (const seq::Status* st = backend_.sequencerStatus()) {
                seq::Request req{};
                req.action = seq::Action::SetQuantize;
                req.value = st->quantize ? 0 : 1;
                const auto res = backend_.requestSequencer(req);
                status_flash_text_ = res.notice ? res.notice : commandStateLabel(res.state);
            }
            rebuildActivePanel();
            return;
        case ControlTag::SeqLiveTransposeBtn:
            if (const seq::Status* st = backend_.sequencerStatus()) {
                seq::Request req{};
                req.action = seq::Action::SetLive;
                req.value = st->midi.live ? 0 : 1;
                const auto res = backend_.requestSequencer(req);
                status_flash_text_ = res.notice ? res.notice : commandStateLabel(res.state);
            }
            rebuildActivePanel();
            return;
        case ControlTag::SeqStepRecBtn:
            if (const seq::Status* st = backend_.sequencerStatus()) {
                seq::Request req{};
                req.action = seq::Action::SetInputMode;
                req.value = static_cast<int16_t>(st->midi.mode == seq::InputMode::StepRecord
                                                     ? seq::InputMode::Normal
                                                     : seq::InputMode::StepRecord);
                const auto res = backend_.requestSequencer(req);
                status_flash_text_ = res.notice ? res.notice : commandStateLabel(res.state);
            }
            rebuildActivePanel();
            return;
        case ControlTag::SeqDivisionBtn:
            if (const seq::Status* st = backend_.sequencerStatus()) {
                const tempo::Division valid_divs[6] = {
                    tempo::Division::Quarter, tempo::Division::Eighth, tempo::Division::EighthTriplet,
                    tempo::Division::Sixteenth, tempo::Division::SixteenthTriplet, tempo::Division::ThirtySecond
                };
                uint8_t idx = 3;
                for (uint8_t i = 0; i < 6; ++i) {
                    if (valid_divs[i] == st->division) {
                        idx = static_cast<uint8_t>((i + 1U) % 6U);
                        break;
                    }
                }
                seq::Request req{};
                req.action = seq::Action::SetDivision;
                req.value = static_cast<int16_t>(valid_divs[idx]);
                const auto res = backend_.requestSequencer(req);
                status_flash_text_ = res.notice ? res.notice : commandStateLabel(res.state);
            }
            rebuildActivePanel();
            return;
        case ControlTag::ArpTransportBtn: {
            const auto res = backend_.requestArpTransport(b.arg0 != 0);
            status_flash_text_ = res.notice ? res.notice : commandStateLabel(res.state);
            rebuildActivePanel();
            return;
        }
        case ControlTag::ArpLatchClearBtn: {
            const auto res = backend_.requestClearLatch();
            status_flash_text_ = res.notice ? res.notice : commandStateLabel(res.state);
            rebuildActivePanel();
            return;
        }
        case ControlTag::PatternSlotBtn:
            selected_pattern_slot_ = static_cast<uint8_t>(std::clamp<int32_t>(b.arg0, 0, 7));
            rebuildActivePanel();
            return;
        case ControlTag::PatternLoadBtn: {
            PatternOperation op{};
            op.action = PatternAction::Load;
            op.slot = selected_pattern_slot_;
            backend_.requestPattern(op);
            rebuildActivePanel();
            return;
        }
        case ControlTag::PatternSaveBtn: {
            PatternOperation op{};
            op.action = PatternAction::Save;
            op.slot = selected_pattern_slot_;
            std::snprintf(op.name, sizeof(op.name), "PAT %u", unsigned(selected_pattern_slot_ + 1U));
            backend_.requestPattern(op);
            rebuildActivePanel();
            return;
        }
        case ControlTag::PatternDeleteBtn: {
            PatternOperation op{};
            op.action = PatternAction::Delete;
            op.slot = selected_pattern_slot_;
            backend_.requestPattern(op);
            rebuildActivePanel();
            return;
        }
        case ControlTag::SceneSlotBtn: {
            const uint8_t slot = static_cast<uint8_t>(std::clamp<int32_t>(b.arg0, 0, 7));
            selected_scene_slot_ = slot;
            const auto* sc = backend_.sceneSlot(slot);
            if (sc && sc->state == seq::SlotState::Valid) {
                SceneOperation op{};
                op.action = SceneAction::Load;
                op.slot = slot;
                op.domains = scene::kAll;
                op.locks = sessionLockMask();
                op.apply_tempo = false;
                backend_.requestScene(op);
            }
            rebuildActivePanel();
            return;
        }
        case ControlTag::SceneRecallBtn: {
            SceneOperation op{};
            op.action = SceneAction::Load;
            op.slot = selected_scene_slot_;
            op.domains = (b.arg1 != 0) ? selective_capture_mask_ : scene::kAll;
            op.locks = sessionLockMask();
            op.apply_tempo = (selective_capture_mask_ & scene::Tempo) != 0U;
            backend_.requestScene(op);
            if (b.arg1 != 0) closeOverlay();
            rebuildActivePanel();
            return;
        }
        case ControlTag::SceneSaveBtn: {
            SceneOperation op{};
            op.action = SceneAction::Save;
            op.slot = selected_scene_slot_;
            op.domains = scene::kAll;
            std::snprintf(op.name, sizeof(op.name), "CENA %u", unsigned(selected_scene_slot_ + 1U));
            backend_.requestScene(op);
            rebuildActivePanel();
            return;
        }
        case ControlTag::SceneClearBtn: {
            SceneOperation op{};
            op.action = SceneAction::Delete;
            op.slot = selected_scene_slot_;
            backend_.requestScene(op);
            rebuildActivePanel();
            return;
        }
        case ControlTag::SceneModalBtn:
            openOverlay(OverlayKind::SceneSelectiveModal, selected_scene_slot_);
            return;
        case ControlTag::SceneDomainBtn:
            selective_capture_mask_ ^= static_cast<uint8_t>(b.arg0);
            if (selective_capture_mask_ == 0U) selective_capture_mask_ = scene::Patch;
            rebuildOverlay();
            return;
        case ControlTag::PerfLockBtn: {
            const uint8_t next = static_cast<uint8_t>(sessionLockMask() ^ static_cast<uint8_t>(b.arg0));
            setSessionLockMask(next);
            rebuildActivePanel();
            return;
        }
        case ControlTag::PatchBankSelectBtn: {
            size_t cnt = 0;
            const auto* cat = UiModel::catalog(cnt);
            if (b.arg1 == 2) {
                const uint8_t max_page = static_cast<uint8_t>((cnt + 5U) / 6U);
                if (b.arg0 < 0) patch_bank_page_ = (patch_bank_page_ == 0U) ? (max_page - 1U) : (patch_bank_page_ - 1U);
                else patch_bank_page_ = static_cast<uint8_t>((patch_bank_page_ + 1U) % std::max<uint8_t>(1U, max_page));
                rebuildActivePanel();
                return;
            }
            uint16_t target_id = static_cast<uint16_t>(b.arg0);
            if (b.arg1 == 1 && cnt > 0U) {
                const uint16_t cur_id = backend_.model().activePatch().id;
                size_t cur_idx = 0U;
                for (size_t i = 0; i < cnt; ++i) {
                    if (cat[i].id == cur_id) { cur_idx = i; break; }
                }
                for (size_t step = 1; step <= cnt; ++step) {
                    const size_t cand = (b.arg0 > 0) ? ((cur_idx + step) % cnt) : ((cur_idx + cnt - step) % cnt);
                    if (backend_.isPatchSelectable(cat[cand].id)) {
                        target_id = cat[cand].id;
                        break;
                    }
                }
            }
            const auto res = backend_.requestSelectPatch(target_id, 0, sessionLockMask());
            pending_discard_patch_ = backend_.patchNeedsDiscardConfirmation() ? target_id : 0xFFFFU;
            status_flash_text_ = res.notice ? res.notice : commandStateLabel(res.state);
            rebuildActivePanel();
            return;
        }
        case ControlTag::PatchDiscardConfirmBtn: {
            const uint16_t target = static_cast<uint16_t>(b.arg0);
            const auto res = backend_.confirmPatchDiscard(target, 0, sessionLockMask());
            pending_discard_patch_ = 0xFFFFU;
            status_flash_text_ = res.notice ? res.notice : commandStateLabel(res.state);
            rebuildActivePanel();
            return;
        }
        case ControlTag::SynthSubTabBtn:
            selectSynthSubTab(static_cast<SynthSubTab>(b.arg0));
            return;
        case ControlTag::BpmDeltaBtn: {
            const int32_t next = std::clamp<int32_t>(backend_.model().getInt("perf.bpm", 120) + b.arg0, 40, 240);
            const auto res = backend_.requestParameterInt("perf.bpm", next);
            status_flash_text_ = res.notice ? res.notice : commandStateLabel(res.state);
            if (overlay_status_label_) {
                char bbuf[48];
                std::snprintf(bbuf, sizeof(bbuf), "%d BPM", int(backend_.model().getInt("perf.bpm", next)));
                lv_label_set_text(overlay_status_label_, bbuf);
            }
            refreshHeaderDynamic();
            return;
        }
        case ControlTag::BpmTapBtn:
            backend_.tapTempo(last_service_ms_ + 500U);
            last_service_ms_ += 500U;
            if (overlay_status_label_) {
                char bbuf[48];
                std::snprintf(bbuf, sizeof(bbuf), "%d BPM", int(backend_.model().getInt("perf.bpm", 120)));
                lv_label_set_text(overlay_status_label_, bbuf);
            }
            refreshHeaderDynamic();
            return;
        case ControlTag::BpmSaveBtn: {
            const auto res = backend_.requestSaveTempo();
            status_flash_text_ = res.notice ? res.notice : commandStateLabel(res.state);
            refreshDynamicLabels();
            return;
        }
        case ControlTag::OverlayHarmApplyTouchBtn: {
            const bool cur = backend_.model().getBool("harmony.apply_touch", true);
            const auto res = backend_.requestParameterBool("harmony.apply_touch", !cur);
            status_flash_text_ = res.notice ? res.notice : commandStateLabel(res.state);
            rebuildOverlay();
            return;
        }
        case ControlTag::OverlayHarmPresetBtn: {
            const auto res = backend_.requestParameterValue("harmony.preset", ParameterValue::makeEnum(b.arg0));
            status_flash_text_ = res.notice ? res.notice : commandStateLabel(res.state);
            rebuildOverlay();
            return;
        }
        case ControlTag::MidiActionBtn:
            backend_.midiPerformanceAction(static_cast<IUiBackend::MidiAction>(b.arg0), 0U);
            rebuildActivePanel();
            return;
        default:
            return;
    }
}

void UiShell018A::maybeReleaseVirtualKeys(AreaId old_area, TocarMode old_mode) {
    if (old_area == AreaId::Tocar && old_mode == TocarMode::VirtualKeys) {
        // Virtual keyboard LV_EVENT_DELETE automatically releases held touch keys;
        // increment counter to track touch-only cleanup without interrupting USB/SEQ/ARP.
        ++metrics_.ui_note_releases;
    }
}

uint32_t UiShell018A::countSubtree(const lv_obj_t* obj) {
    if (!obj) return 0U;
    uint32_t count = 1U;
    const uint32_t child_cnt = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < child_cnt; ++i) {
        count += countSubtree(lv_obj_get_child(obj, i));
    }
    return count;
}

uint32_t UiShell018A::countAllObjects() const {
    return countSubtree(root_);
}

LvglHeapStats UiShell018A::readLvglHeapStats() {
    lv_mem_monitor_t mon{};
    lv_mem_monitor(&mon);
    LvglHeapStats s{};
    s.total_size = static_cast<uint32_t>(mon.total_size);
    s.free_bytes = static_cast<uint32_t>(mon.free_size);
    s.free_biggest_size = static_cast<uint32_t>(mon.free_biggest_size);
    s.used_pct = static_cast<uint32_t>(mon.used_pct);
    s.max_used = static_cast<uint32_t>(mon.max_used);
    s.frag_pct = static_cast<uint32_t>(mon.frag_pct);
    return s;
}

LayoutBox UiShell018A::headerBox() const {
    LayoutBox r{};
    if (!header_) return r;
    lv_area_t a{};
    lv_obj_get_coords(header_, &a);
    r = {a.x1, a.y1, a.x2 - a.x1 + 1, a.y2 - a.y1 + 1, !lv_obj_has_flag(header_, LV_OBJ_FLAG_HIDDEN)};
    return r;
}

LayoutBox UiShell018A::sidebarBox() const {
    LayoutBox r{};
    if (!sidebar_) return r;
    lv_area_t a{};
    lv_obj_get_coords(sidebar_, &a);
    r = {a.x1, a.y1, a.x2 - a.x1 + 1, a.y2 - a.y1 + 1, !lv_obj_has_flag(sidebar_, LV_OBJ_FLAG_HIDDEN)};
    return r;
}

LayoutBox UiShell018A::panelBox() const {
    LayoutBox r{};
    if (!panel_) return r;
    lv_area_t a{};
    lv_obj_get_coords(panel_, &a);
    r = {a.x1, a.y1, a.x2 - a.x1 + 1, a.y2 - a.y1 + 1, !lv_obj_has_flag(panel_, LV_OBJ_FLAG_HIDDEN)};
    return r;
}

LayoutBox UiShell018A::footerBox() const {
    LayoutBox r{};
    if (!footer_) return r;
    lv_area_t a{};
    lv_obj_get_coords(footer_, &a);
    r = {a.x1, a.y1, a.x2 - a.x1 + 1, a.y2 - a.y1 + 1, !lv_obj_has_flag(footer_, LV_OBJ_FLAG_HIDDEN)};
    return r;
}

LayoutBox UiShell018A::panicBox() const {
    LayoutBox r{};
    if (!hdr_panic_btn_) return r;
    lv_area_t a{};
    lv_obj_get_coords(hdr_panic_btn_, &a);
    r = {a.x1, a.y1, a.x2 - a.x1 + 1, a.y2 - a.y1 + 1, !lv_obj_has_flag(hdr_panic_btn_, LV_OBJ_FLAG_HIDDEN)};
    return r;
}

bool UiShell018A::headerTargetsDisjoint(int32_t* min_gap_out) const {
    lv_obj_t* targets[7] = {
        hdr_bpm_btn_, hdr_harm_btn_, hdr_q_btn_, hdr_ch_btn_, hdr_variant_btn_, hdr_panic_btn_, hdr_mode_badge_
    };
    lv_area_t rects[7]{};
    int count = 0;
    for (lv_obj_t* obj : targets) {
        if (!obj) continue;
        if (!objInsideScreen(obj)) return false;
        lv_obj_get_coords(obj, &rects[count++]);
    }
    int32_t min_gap = 1000;
    for (int i = 0; i < count; ++i) {
        for (int j = i + 1; j < count; ++j) {
            if (boxesOverlap(rects[i], rects[j])) return false;
            const int32_t gx = (rects[i].x2 < rects[j].x1) ? (rects[j].x1 - rects[i].x2 - 1)
                             : (rects[j].x2 < rects[i].x1) ? (rects[i].x1 - rects[j].x2 - 1) : 0;
            const int32_t gy = (rects[i].y2 < rects[j].y1) ? (rects[j].y1 - rects[i].y2 - 1)
                             : (rects[j].y2 < rects[i].y1) ? (rects[i].y1 - rects[j].y2 - 1) : 0;
            const int32_t g = std::max(gx, gy);
            if (g < min_gap) min_gap = g;
        }
    }
    if (min_gap_out) *min_gap_out = min_gap;
    return true;
}

bool UiShell018A::panicAccessibleInOverlay() const {
    if (!hdr_panic_btn_ || !objInsideScreen(hdr_panic_btn_) || !lv_obj_has_flag(hdr_panic_btn_, LV_OBJ_FLAG_CLICKABLE)) {
        return false;
    }
    if (!overlay_backdrop_) return true;
    lv_area_t pa{}, oa{};
    lv_obj_get_coords(hdr_panic_btn_, &pa);
    lv_obj_get_coords(overlay_backdrop_, &oa);
    return !boxesOverlap(pa, oa);
}

lv_obj_t* UiShell018A::footerButton(AreaId area) const {
    const uint8_t idx = static_cast<uint8_t>(area);
    return idx < footer_btns_.size() ? footer_btns_[idx] : nullptr;
}

lv_obj_t* UiShell018A::sidebarButton(uint8_t index) const {
    return index < sidebar_btns_.size() ? sidebar_btns_[index] : nullptr;
}

}  // namespace smk::p4::ui::proto018a
