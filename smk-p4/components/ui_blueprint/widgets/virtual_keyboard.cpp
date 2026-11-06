#include "ui_widgets.h"
#include <algorithm>
#include <cstdio>

namespace smk::p4::ui {

namespace {

constexpr uint8_t kWhiteOffsets[7] = {0, 2, 4, 5, 7, 9, 11};
constexpr const char* kWhiteNames[7] = {"C", "D", "E", "F", "G", "A", "B"};
constexpr size_t kMaxKeyboardKeys = 32; // Max 2 octaves = 15 white + 10 black = 25 keys

struct KeyboardState;

struct KeySlot {
    KeyboardState* owner           = nullptr;
    lv_obj_t*      btn             = nullptr;
    lv_obj_t*      lbl             = nullptr;
    uint8_t        semitone_offset = 0;
    uint8_t        degree          = 0;
    bool           is_black        = false;
    bool           pressed         = false;
    uint8_t        active_note     = 60;
    uint32_t       active_token    = 0;
};

struct KeyboardState {
    IUiBackend*          backend    = nullptr;
    UiNavigationManager* nav        = nullptr;
    lv_obj_t*            box        = nullptr;
    KeyboardDisplayMode  mode       = KeyboardDisplayMode::StagePlay;
    KeySlot              keys[kMaxKeyboardKeys]{};
    size_t               key_count  = 0;
};

uint8_t computeStartNote(const IUiBackend& backend, KeyboardDisplayMode mode) {
    const int32_t oct_shift = std::clamp<int32_t>(backend.model().getInt("perf.octave_shift", 0), -2, 2);
    if (mode == KeyboardDisplayMode::StagePlay) {
        return static_cast<uint8_t>(std::clamp<int32_t>(60 + oct_shift * 12, 24, 96));
    }
    if (mode == KeyboardDisplayMode::StageKeys2Oct) {
        return static_cast<uint8_t>(std::clamp<int32_t>(48 + oct_shift * 12, 24, 84));
    }
    return 48; // C3..C5 fixed reference for SplitZones and HarmonyView
}

uint32_t resolveKeyColor(const IUiBackend& backend, KeyboardDisplayMode mode,
                         uint8_t note, bool is_black, bool is_pressed) {
    if (is_pressed || backend.isNoteHeld(note)) {
        return palette::kAccentPrimary;
    }

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
        const uint8_t pc = note % 12;
        if (harm.scale_mask_12 & (1u << pc)) {
            return is_black ? 0x2B4252 : 0xB8D4E3;
        }
        return is_black ? 0x10151A : 0x4A5862;
    }

    return base_col;
}

void updateKeyVisuals(KeyboardState& state) {
    if (!state.backend) return;
    const uint8_t start_note = computeStartNote(*state.backend, state.mode);
    for (size_t i = 0; i < state.key_count; ++i) {
        KeySlot& slot = state.keys[i];
        if (!slot.btn) continue;

        // Sync pressed state if backend cleared notes externally (e.g. Panic)
        if (slot.pressed && !state.backend->isNoteHeld(slot.active_note)) {
            slot.pressed = false;
            slot.active_token = 0;
            lv_obj_remove_state(slot.btn, LV_STATE_PRESSED);
        }

        const uint8_t display_note = static_cast<uint8_t>(start_note + slot.semitone_offset);
        const uint8_t color_note = slot.pressed ? slot.active_note : display_note;
        const uint32_t col = resolveKeyColor(*state.backend, state.mode, color_note, slot.is_black, slot.pressed);
        lv_obj_set_style_bg_color(slot.btn, lv_color_hex(col), 0);

        if (slot.lbl && !slot.is_black) {
            char nbuf[8];
            std::snprintf(nbuf, sizeof(nbuf), "%s%d", kWhiteNames[slot.degree], int(display_note / 12) - 1);
            lv_label_set_text(slot.lbl, nbuf);
        }
    }
}

void releaseActiveSlots(KeyboardState& state, bool update_visuals=true) {
    if (!state.backend) return;
    for (size_t i = 0; i < state.key_count; ++i) {
        KeySlot& slot = state.keys[i];
        if (slot.pressed) {
            state.backend->requestNoteOff(slot.active_note, slot.active_token);
            slot.pressed = false;
            slot.active_token = 0;
            if (update_visuals && slot.btn) {
                lv_obj_remove_state(slot.btn, LV_STATE_PRESSED);
            }
        }
    }
}

void onKeyboardKeyEvent(lv_event_t* e) {
    const lv_event_code_t code = lv_event_get_code(e);
    auto* slot = static_cast<KeySlot*>(lv_event_get_user_data(e));
    if (!slot || !slot->owner || !slot->owner->backend) return;
    KeyboardState& state = *slot->owner;
    if (code == LV_EVENT_DELETE) {
        if (slot->pressed) state.backend->requestNoteOff(slot->active_note, slot->active_token);
        slot->pressed=false; slot->active_token=0; slot->btn=nullptr; slot->lbl=nullptr;
        return;
    }
    // Split and harmony keyboards are visual previews; never send live notes.
    if (!state.backend->isDemoMode() &&
        (state.mode == KeyboardDisplayMode::SplitZones ||
         (state.mode == KeyboardDisplayMode::HarmonyView && !state.backend->model().live_harmony.available))) return;

    if (code == LV_EVENT_PRESSED) {
        // Guard against duplicate PRESSED without an intervening release
        if (slot->pressed) {
            return;
        }
        const uint8_t start_note = computeStartNote(*state.backend, state.mode);
        const uint8_t note = static_cast<uint8_t>(start_note + slot->semitone_offset);
        const UiCommandResult res = state.backend->requestNoteOn(note, 100);
        if (res.accepted()) {
            slot->pressed = true;
            slot->active_note = note;
            slot->active_token = res.token;
        }
        state.backend->model().setLastPreviewNote(note);
        // Safe in-place visual update (never call destructive navigateTo inside key callback)
        updateKeyVisuals(state);
        if (state.nav) {
            state.nav->refreshActive();
        }
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if (!slot->pressed) {
            return;
        }
        const uint8_t note_to_release = slot->active_note;
        const uint32_t token_to_release = slot->active_token;
        slot->pressed = false;
        slot->active_token = 0;
        state.backend->requestNoteOff(note_to_release, token_to_release);
        updateKeyVisuals(state);
        if (state.nav) {
            state.nav->refreshActive();
        }
    }
    // Note: LV_EVENT_CLICKED is intentionally ignored so a tap never triggers a duplicate Note On.
}

} // namespace

lv_obj_t* createVirtualKeyboard(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav,
                                KeyboardDisplayMode mode, int32_t x, int32_t y, int32_t w, int32_t h) {
    if (!parent) return nullptr;
    lv_obj_t* box = lv_obj_create(parent);
    if (!box) return nullptr;
    applyPanelStyle(box, false);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, w, h);
    lv_obj_set_style_pad_all(box, 2, 0);

    auto* state = new KeyboardState{};
    state->backend = &backend;
    state->nav = &nav;
    state->box = box;
    state->mode = mode;
    lv_obj_set_user_data(box, state);

    lv_obj_add_event_cb(
        box,
        [](lv_event_t* e) {
            if (lv_event_get_code(e) == LV_EVENT_DELETE) {
                auto* st = static_cast<KeyboardState*>(lv_event_get_user_data(e));
                if (st) {
                    // LVGL 9.3 sends parent DELETE before child DELETE. Detach callbacks
                    // referencing KeySlot storage before freeing it; no child accesses here.
                    releaseActiveSlots(*st, false);
                    for (size_t i=0;i<st->key_count;++i) {
                        if (st->keys[i].btn) lv_obj_remove_event_cb(st->keys[i].btn, onKeyboardKeyEvent);
                    }
                    lv_obj_set_user_data(st->box, nullptr);
                    delete st;
                }
            }
        },
        LV_EVENT_DELETE, state);

    const uint8_t start_note = computeStartNote(backend, mode);
    const uint8_t num_octaves = (mode == KeyboardDisplayMode::StagePlay) ? 1 : 2;
    const uint8_t white_count = static_cast<uint8_t>(num_octaves * 7 + 1);

    const int32_t inner_w = w - 8;
    const int32_t key_gap = 2;
    const int32_t white_w = (inner_w - key_gap * (white_count - 1)) / white_count;
    const int32_t white_h = h - 8;
    const int32_t black_w = (white_w * 3) / 5;
    const int32_t black_h = (white_h * 58) / 100;

    // 1. Create white keys first
    for (uint8_t wk = 0; wk < white_count; ++wk) {
        if (state->key_count >= kMaxKeyboardKeys) break;
        const uint8_t oct = wk / 7;
        const uint8_t deg = wk % 7;
        const uint8_t semi_offset = static_cast<uint8_t>(oct * 12 + kWhiteOffsets[deg]);
        const uint8_t note = static_cast<uint8_t>(start_note + semi_offset);
        const int32_t kx = 2 + wk * (white_w + key_gap);

        lv_obj_t* btn = lv_button_create(box);
        if (!btn) continue;
        lv_obj_remove_style_all(btn);
        lv_obj_set_pos(btn, kx, 2);
        lv_obj_set_size(btn, white_w, white_h);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(resolveKeyColor(backend, mode, note, false, false)), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(palette::kAccentPrimary), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(palette::kBorder), 0);
        lv_obj_set_style_radius(btn, 3, 0);
        lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(btn, LV_OBJ_FLAG_PRESS_LOCK);

        lv_obj_t* lbl = nullptr;
        if (deg == 0 || mode == KeyboardDisplayMode::StagePlay || mode == KeyboardDisplayMode::StageKeys2Oct) {
            char nbuf[8];
            std::snprintf(nbuf, sizeof(nbuf), "%s%d", kWhiteNames[deg], int(note / 12) - 1);
            lbl = lv_label_create(btn);
            if (lbl) {
                lv_label_set_text(lbl, nbuf);
                lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
                lv_obj_set_style_text_color(lbl, lv_color_hex(palette::kBackground), 0);
                lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -2);
            }
        }

        KeySlot& slot = state->keys[state->key_count++];
        slot.owner = state;
        slot.btn = btn;
        slot.lbl = lbl;
        slot.semitone_offset = semi_offset;
        slot.degree = deg;
        slot.is_black = false;
        slot.pressed = false;
        slot.active_note = note;
        slot.active_token = 0;

        lv_obj_add_event_cb(btn, onKeyboardKeyEvent, LV_EVENT_ALL, &slot);
    }

    // 2. Create black keys on top
    constexpr uint8_t kBlackAfterWhite[5] = {0, 1, 3, 4, 5};
    constexpr uint8_t kBlackSemis[5] = {1, 3, 6, 8, 10};
    for (uint8_t oct = 0; oct < num_octaves; ++oct) {
        for (uint8_t b = 0; b < 5; ++b) {
            if (state->key_count >= kMaxKeyboardKeys) break;
            const uint8_t wk = static_cast<uint8_t>(oct * 7 + kBlackAfterWhite[b]);
            const uint8_t semi_offset = static_cast<uint8_t>(oct * 12 + kBlackSemis[b]);
            const uint8_t note = static_cast<uint8_t>(start_note + semi_offset);
            const int32_t boundary_x = 2 + (wk + 1) * (white_w + key_gap) - key_gap / 2;
            const int32_t bx = boundary_x - black_w / 2;

            lv_obj_t* btn = lv_button_create(box);
            if (!btn) continue;
            lv_obj_remove_style_all(btn);
            lv_obj_set_pos(btn, bx, 2);
            lv_obj_set_size(btn, black_w, black_h);
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(btn, lv_color_hex(resolveKeyColor(backend, mode, note, true, false)), 0);
            lv_obj_set_style_bg_color(btn, lv_color_hex(palette::kAccentPrimary), LV_STATE_PRESSED);
            lv_obj_set_style_border_width(btn, 1, 0);
            lv_obj_set_style_border_color(btn, lv_color_hex(palette::kBorder), 0);
            lv_obj_set_style_radius(btn, 3, 0);
            lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_remove_flag(btn, LV_OBJ_FLAG_PRESS_LOCK);

            KeySlot& slot = state->keys[state->key_count++];
            slot.owner = state;
            slot.btn = btn;
            slot.lbl = nullptr;
            slot.semitone_offset = semi_offset;
            slot.degree = 0;
            slot.is_black = true;
            slot.pressed = false;
            slot.active_note = note;
            slot.active_token = 0;

            lv_obj_add_event_cb(btn, onKeyboardKeyEvent, LV_EVENT_ALL, &slot);
        }
    }

    return box;
}

void refreshVirtualKeyboard(lv_obj_t* keyboard_box) {
    if (!keyboard_box) return;
    auto* state = static_cast<KeyboardState*>(lv_obj_get_user_data(keyboard_box));
    if (!state) return;
    updateKeyVisuals(*state);
}

void releaseVirtualKeyboardNotes(lv_obj_t* keyboard_box) {
    if (!keyboard_box) return;
    auto* state = static_cast<KeyboardState*>(lv_obj_get_user_data(keyboard_box));
    if (!state) return;
    releaseActiveSlots(*state);
    updateKeyVisuals(*state);
}

} // namespace smk::p4::ui
