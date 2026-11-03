#include "ui_widgets.h"
#include <cstdio>

namespace smk::p4::ui {

namespace {

constexpr uint8_t kWhiteOffsets[7] = {0, 2, 4, 5, 7, 9, 11};
constexpr const char* kWhiteNames[7] = {"C", "D", "E", "F", "G", "A", "B"};

struct KeyCtx {
    UiDemoBackend*       backend = nullptr;
    UiNavigationManager* nav     = nullptr;
    uint8_t              note    = 60;
    KeyboardDisplayMode  mode    = KeyboardDisplayMode::StagePlay;
};

uint32_t resolveKeyColor(const UiDemoBackend& backend, KeyboardDisplayMode mode, uint8_t note, bool is_black) {
    const uint32_t base_col = is_black ? palette::kKeyBlack : palette::kKeyWhite;

    if (mode == KeyboardDisplayMode::SplitZones) {
        const int32_t lmode = backend.model().getInt("perf.layer_mode", 0);
        const int32_t a_low = backend.model().getInt("layer_a.key_low", 36);
        const int32_t a_high = backend.model().getInt("layer_a.key_high", 59);
        const int32_t b_low = backend.model().getInt("layer_b.key_low", 60);
        const int32_t b_high = backend.model().getInt("layer_b.key_high", 96);

        const bool in_a = (lmode != 1) && (note >= a_low && note <= a_high);
        const bool in_b = (lmode >= 1) && (note >= b_low && note <= b_high);
        if (in_a && in_b) return palette::kAccentPrimary;
        if (in_a) return is_black ? 0x1F4E5B : 0x5BA8C4;
        if (in_b) return is_black ? 0x295E4A : palette::kActive;
        return base_col;
    }

    if (mode == KeyboardDisplayMode::HarmonyView) {
        const HarmonyPreviewResult harm = backend.model().computeHarmony(backend.model().lastPreviewNote());
        // Highest priority: input note & quantized note & generated chord notes
        if (note == harm.quantized_note) {
            return palette::kActive; // Quantized root note
        }
        for (uint8_t i = 0; i < harm.chord_count; ++i) {
            if (harm.chord_notes[i] == note) {
                return palette::kAccentPrimary; // Chord member note
            }
        }
        if (note == harm.input_note && !harm.input_in_scale) {
            return palette::kErrorPanic; // Out-of-scale raw input note
        }
        // Highlight scale degrees
        const uint8_t pc = note % 12;
        if (harm.scale_mask_12 & (1u << pc)) {
            return is_black ? 0x2B4252 : 0xB8D4E3;
        }
        return is_black ? 0x10151A : 0x4A5862;
    }

    if (backend.isNoteHeld(note)) {
        return palette::kAccentPrimary;
    }
    return base_col;
}

} // namespace

lv_obj_t* createVirtualKeyboard(lv_obj_t* parent, UiDemoBackend& backend, UiNavigationManager& nav,
                                KeyboardDisplayMode mode, int32_t x, int32_t y, int32_t w, int32_t h) {
    if (!parent) return nullptr;
    lv_obj_t* box = lv_obj_create(parent);
    if (!box) return nullptr;
    applyPanelStyle(box, false);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, w, h);
    lv_obj_set_style_pad_all(box, 2, 0);

    const int32_t oct_shift = backend.model().getInt("perf.octave_shift", 0);
    const uint8_t base_note = static_cast<uint8_t>(48 + (oct_shift + 1) * 12); // Default C4 = 60
    const uint8_t num_octaves = (mode == KeyboardDisplayMode::StagePlay) ? 1 : 2;
    const uint8_t start_note = (mode == KeyboardDisplayMode::StagePlay) ? base_note : 48; // C3..C5 for 2-octave views
    const uint8_t white_count = static_cast<uint8_t>(num_octaves * 7 + 1);

    const int32_t inner_w = w - 8;
    const int32_t key_gap = 2;
    const int32_t white_w = (inner_w - key_gap * (white_count - 1)) / white_count;
    const int32_t white_h = h - 8;
    const int32_t black_w = (white_w * 3) / 5;
    const int32_t black_h = (white_h * 58) / 100;

    auto key_cb = [](lv_event_t* e) {
        const lv_event_code_t code = lv_event_get_code(e);
        auto* ctx = static_cast<KeyCtx*>(lv_event_get_user_data(e));
        if (!ctx || !ctx->backend) return;
        if (code == LV_EVENT_PRESSED || code == LV_EVENT_CLICKED) {
            ctx->backend->triggerNoteOn(ctx->note, 100);
            if (ctx->mode == KeyboardDisplayMode::HarmonyView && ctx->nav) {
                ctx->nav->navigateTo(ctx->nav->currentScreen(), ctx->nav->subTab(ctx->nav->currentScreen()));
            }
        } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
            ctx->backend->triggerNoteOff(ctx->note);
        }
    };

    // Create white keys first
    for (uint8_t wk = 0; wk < white_count; ++wk) {
        const uint8_t oct = wk / 7;
        const uint8_t deg = wk % 7;
        const uint8_t note = static_cast<uint8_t>(start_note + oct * 12 + kWhiteOffsets[deg]);
        const int32_t kx = 2 + wk * (white_w + key_gap);

        lv_obj_t* btn = lv_button_create(box);
        if (!btn) continue;
        lv_obj_remove_style_all(btn);
        lv_obj_set_pos(btn, kx, 2);
        lv_obj_set_size(btn, white_w, white_h);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(resolveKeyColor(backend, mode, note, false)), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(palette::kAccentPrimary), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(palette::kBorder), 0);
        lv_obj_set_style_radius(btn, 3, 0);
        lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_PRESS_LOCK);

        if (deg == 0 || mode == KeyboardDisplayMode::StagePlay) {
            char nbuf[8];
            std::snprintf(nbuf, sizeof(nbuf), "%s%u", kWhiteNames[deg], (note / 12) - 1);
            lv_obj_t* lbl = lv_label_create(btn);
            if (lbl) {
                lv_label_set_text(lbl, nbuf);
                lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
                lv_obj_set_style_text_color(lbl, lv_color_hex(palette::kBackground), 0);
                lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -2);
            }
        }
        attachLvContext<KeyCtx>(btn, KeyCtx{&backend, &nav, note, mode}, key_cb, LV_EVENT_ALL);
    }

    // Create black keys on top
    constexpr uint8_t kBlackAfterWhite[5] = {0, 1, 3, 4, 5};
    constexpr uint8_t kBlackSemis[5] = {1, 3, 6, 8, 10};
    for (uint8_t oct = 0; oct < num_octaves; ++oct) {
        for (uint8_t b = 0; b < 5; ++b) {
            const uint8_t wk = static_cast<uint8_t>(oct * 7 + kBlackAfterWhite[b]);
            const uint8_t note = static_cast<uint8_t>(start_note + oct * 12 + kBlackSemis[b]);
            const int32_t boundary_x = 2 + (wk + 1) * (white_w + key_gap) - key_gap / 2;
            const int32_t bx = boundary_x - black_w / 2;

            lv_obj_t* btn = lv_button_create(box);
            if (!btn) continue;
            lv_obj_remove_style_all(btn);
            lv_obj_set_pos(btn, bx, 2);
            lv_obj_set_size(btn, black_w, black_h);
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(btn, lv_color_hex(resolveKeyColor(backend, mode, note, true)), 0);
            lv_obj_set_style_bg_color(btn, lv_color_hex(palette::kAccentPrimary), LV_STATE_PRESSED);
            lv_obj_set_style_border_width(btn, 1, 0);
            lv_obj_set_style_border_color(btn, lv_color_hex(palette::kBorder), 0);
            lv_obj_set_style_radius(btn, 3, 0);
            lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_PRESS_LOCK);

            attachLvContext<KeyCtx>(btn, KeyCtx{&backend, &nav, note, mode}, key_cb, LV_EVENT_ALL);
        }
    }

    return box;
}

} // namespace smk::p4::ui
