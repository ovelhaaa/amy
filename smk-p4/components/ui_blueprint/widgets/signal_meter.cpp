#include "ui_widgets.h"
#include <cstdio>

namespace smk::p4::ui {

lv_obj_t* createSignalMeter(lv_obj_t* parent, const char* title, uint8_t left_pct, uint8_t right_pct,
                            bool is_simulated, int32_t x, int32_t y, int32_t w, int32_t h) {
    if (!parent) return nullptr;
    lv_obj_t* box = lv_obj_create(parent);
    if (!box) return nullptr;
    applyPanelStyle(box, false);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, w, h);

    char hdr[48];
    std::snprintf(hdr, sizeof(hdr), "%s %s", title ? title : "BUS", is_simulated ? "[SIM DEMO]" : "");
    createLabel(box, hdr, 6, 2, w - 16, &lv_font_montserrat_14,
                is_simulated ? palette::kWarning : palette::kTextPrimary);

    // L bar
    createLabel(box, "L", 6, 24, 16, &lv_font_montserrat_14, palette::kTextSecondary);
    lv_obj_t* bar_l = lv_bar_create(box);
    if (bar_l) {
        lv_obj_set_pos(bar_l, 24, 26);
        lv_obj_set_size(bar_l, w - 40, 12);
        lv_bar_set_range(bar_l, 0, 100);
        lv_bar_set_value(bar_l, left_pct, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(bar_l, lv_color_hex(palette::kBackground), LV_PART_MAIN);
        lv_obj_set_style_bg_color(bar_l, lv_color_hex(left_pct > 85 ? palette::kWarning : palette::kActive),
                                  LV_PART_INDICATOR);
    }

    // R bar
    createLabel(box, "R", 6, 42, 16, &lv_font_montserrat_14, palette::kTextSecondary);
    lv_obj_t* bar_r = lv_bar_create(box);
    if (bar_r) {
        lv_obj_set_pos(bar_r, 24, 44);
        lv_obj_set_size(bar_r, w - 40, 12);
        lv_bar_set_range(bar_r, 0, 100);
        lv_bar_set_value(bar_r, right_pct, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(bar_r, lv_color_hex(palette::kBackground), LV_PART_MAIN);
        lv_obj_set_style_bg_color(bar_r, lv_color_hex(right_pct > 85 ? palette::kWarning : palette::kActive),
                                  LV_PART_INDICATOR);
    }

    return box;
}

} // namespace smk::p4::ui
