#include "ui_widgets.h"
#include <algorithm>
#include <cstdio>

namespace smk::p4::ui {

lv_obj_t* createEnvelopeEditor(lv_obj_t* parent, UiDemoBackend& backend,
                               int32_t x, int32_t y, int32_t w, int32_t h) {
    if (!parent) return nullptr;
    lv_obj_t* box = lv_obj_create(parent);
    if (!box) return nullptr;
    applyPanelStyle(box, false);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, w, h);

    const float atk_ms = backend.model().getFloat("env.amp.attack_ms", 15.0f);
    const float dec_ms = backend.model().getFloat("env.amp.decay_ms", 350.0f);
    const float sus_pct = backend.model().getFloat("env.amp.sustain", 75.0f);
    const float rel_ms = backend.model().getFloat("env.amp.release_ms", 280.0f);

    char hdr[96];
    std::snprintf(hdr, sizeof(hdr),
                  "ADSR CONTOUR PREVIEW (DEMO)   A:%.0fms  D:%.0fms  S:%.0f%%  R:%.0fms",
                  static_cast<double>(atk_ms), static_cast<double>(dec_ms),
                  static_cast<double>(sus_pct), static_cast<double>(rel_ms));
    createLabel(box, hdr, 8, 4, w - 92, &lv_font_montserrat_14, palette::kAccentPrimary);
    createStatusBadge(box, FeatureStatus::Planned, w - 80, 4);

    // Visual ADSR stage bars proportional to Attack, Decay, Sustain, Release
    const int32_t graph_y = 28;
    const int32_t graph_h = h - 42;
    const int32_t graph_w = w - 24;

    const int32_t seg_w = (graph_w - 18) / 4;
    const int32_t atk_pct = std::clamp<int32_t>(static_cast<int32_t>((atk_ms / 2000.0f) * 100.0f) + 15, 15, 100);
    const int32_t dec_pct = std::clamp<int32_t>(static_cast<int32_t>((dec_ms / 2500.0f) * 100.0f) + 15, 15, 100);
    const int32_t sus_i   = std::clamp<int32_t>(static_cast<int32_t>(sus_pct), 5, 100);
    const int32_t rel_pct = std::clamp<int32_t>(static_cast<int32_t>((rel_ms / 3000.0f) * 100.0f) + 15, 15, 100);

    const int32_t vals[4] = {atk_pct, dec_pct, sus_i, rel_pct};
    static constexpr const char* kStageNames[4] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
    static constexpr uint32_t kStageCols[4] = {
        palette::kAccentPrimary, palette::kActive, palette::kActive, palette::kWarning
    };

    for (int i = 0; i < 4; ++i) {
        const int32_t sx = 8 + i * (seg_w + 6);
        lv_obj_t* track = lv_bar_create(box);
        if (!track) continue;
        lv_obj_set_pos(track, sx, graph_y);
        lv_obj_set_size(track, seg_w, graph_h - 20);
        lv_bar_set_range(track, 0, 100);
        lv_bar_set_value(track, vals[i], LV_ANIM_OFF);
        lv_obj_set_style_bg_color(track, lv_color_hex(palette::kBackground), LV_PART_MAIN);
        lv_obj_set_style_bg_color(track, lv_color_hex(kStageCols[i]), LV_PART_INDICATOR);
        lv_obj_set_style_radius(track, 4, LV_PART_MAIN);
        lv_obj_set_style_radius(track, 4, LV_PART_INDICATOR);

        createLabel(box, kStageNames[i], sx + 4, graph_y + graph_h - 18, seg_w - 8,
                    &lv_font_montserrat_14, palette::kTextSecondary);
    }

    return box;
}

} // namespace smk::p4::ui
