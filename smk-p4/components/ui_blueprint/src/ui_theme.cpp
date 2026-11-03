#include "ui_theme.h"

namespace smk::p4::ui {

namespace {
ThemeStyles g_theme{};
} // namespace

const char* featureStatusLabel(FeatureStatus status) {
    switch (status) {
        case FeatureStatus::Available:   return "AVAIL";
        case FeatureStatus::Planned:     return "PLAN";
        case FeatureStatus::Unavailable: return "N/A";
        case FeatureStatus::Demo:        return "DEMO";
        default:                         return "UNK";
    }
}

uint32_t featureStatusColor(FeatureStatus status) {
    switch (status) {
        case FeatureStatus::Available:   return palette::kActive;
        case FeatureStatus::Planned:     return palette::kWarning;
        case FeatureStatus::Unavailable: return palette::kTextSecondary;
        case FeatureStatus::Demo:        return palette::kAccentPrimary;
        default:                         return palette::kBorder;
    }
}

void ThemeStyles::init() {
    if (initialized) return;
    initialized = true;

    lv_style_init(&screen);
    lv_style_set_bg_color(&screen, lv_color_hex(palette::kBackground));
    lv_style_set_bg_opa(&screen, LV_OPA_COVER);
    lv_style_set_text_color(&screen, lv_color_hex(palette::kTextPrimary));
    lv_style_set_text_font(&screen, &lv_font_montserrat_14);
    lv_style_set_border_width(&screen, 0);
    lv_style_set_pad_all(&screen, 0);
    lv_style_set_radius(&screen, 0);

    lv_style_init(&panel);
    lv_style_set_bg_color(&panel, lv_color_hex(palette::kPanel));
    lv_style_set_bg_opa(&panel, LV_OPA_COVER);
    lv_style_set_border_color(&panel, lv_color_hex(palette::kBorder));
    lv_style_set_border_width(&panel, 1);
    lv_style_set_radius(&panel, 6);
    lv_style_set_pad_all(&panel, 6);
    lv_style_set_text_color(&panel, lv_color_hex(palette::kTextPrimary));

    lv_style_init(&panel_highlight);
    lv_style_set_bg_color(&panel_highlight, lv_color_hex(palette::kPanelHighlight));
    lv_style_set_bg_opa(&panel_highlight, LV_OPA_COVER);
    lv_style_set_border_color(&panel_highlight, lv_color_hex(palette::kAccentPrimary));
    lv_style_set_border_width(&panel_highlight, 1);
    lv_style_set_radius(&panel_highlight, 6);
    lv_style_set_pad_all(&panel_highlight, 6);
    lv_style_set_text_color(&panel_highlight, lv_color_hex(palette::kTextPrimary));

    lv_style_init(&button);
    lv_style_set_bg_color(&button, lv_color_hex(palette::kPanelHighlight));
    lv_style_set_bg_opa(&button, LV_OPA_COVER);
    lv_style_set_border_color(&button, lv_color_hex(palette::kBorder));
    lv_style_set_border_width(&button, 1);
    lv_style_set_radius(&button, 5);
    lv_style_set_shadow_width(&button, 0);
    lv_style_set_text_color(&button, lv_color_hex(palette::kTextPrimary));
    lv_style_set_pad_all(&button, 4);

    lv_style_init(&button_active);
    lv_style_set_bg_color(&button_active, lv_color_hex(palette::kActive));
    lv_style_set_bg_opa(&button_active, LV_OPA_COVER);
    lv_style_set_border_color(&button_active, lv_color_hex(palette::kActive));
    lv_style_set_border_width(&button_active, 1);
    lv_style_set_text_color(&button_active, lv_color_hex(palette::kBackground));

    lv_style_init(&button_accent);
    lv_style_set_bg_color(&button_accent, lv_color_hex(palette::kAccentPrimary));
    lv_style_set_bg_opa(&button_accent, LV_OPA_COVER);
    lv_style_set_border_color(&button_accent, lv_color_hex(palette::kAccentPrimary));
    lv_style_set_border_width(&button_accent, 1);
    lv_style_set_text_color(&button_accent, lv_color_hex(palette::kBackground));

    lv_style_init(&button_panic);
    lv_style_set_bg_color(&button_panic, lv_color_hex(palette::kErrorPanic));
    lv_style_set_bg_opa(&button_panic, LV_OPA_COVER);
    lv_style_set_border_color(&button_panic, lv_color_hex(0xE57373));
    lv_style_set_border_width(&button_panic, 1);
    lv_style_set_radius(&button_panic, 5);
    lv_style_set_shadow_width(&button_panic, 0);
    lv_style_set_text_color(&button_panic, lv_color_hex(palette::kTextPrimary));

    lv_style_init(&tab_button);
    lv_style_set_bg_color(&tab_button, lv_color_hex(palette::kPanel));
    lv_style_set_bg_opa(&tab_button, LV_OPA_COVER);
    lv_style_set_border_color(&tab_button, lv_color_hex(palette::kBorder));
    lv_style_set_border_width(&tab_button, 1);
    lv_style_set_radius(&tab_button, 4);
    lv_style_set_shadow_width(&tab_button, 0);
    lv_style_set_text_color(&tab_button, lv_color_hex(palette::kTextSecondary));

    lv_style_init(&tab_button_active);
    lv_style_set_bg_color(&tab_button_active, lv_color_hex(palette::kPanelHighlight));
    lv_style_set_border_color(&tab_button_active, lv_color_hex(palette::kAccentPrimary));
    lv_style_set_border_width(&tab_button_active, 2);
    lv_style_set_text_color(&tab_button_active, lv_color_hex(palette::kAccentPrimary));

    lv_style_init(&badge);
    lv_style_set_bg_opa(&badge, LV_OPA_COVER);
    lv_style_set_radius(&badge, 3);
    lv_style_set_pad_hor(&badge, 4);
    lv_style_set_pad_ver(&badge, 1);
    lv_style_set_border_width(&badge, 0);

    lv_style_init(&slider_main);
    lv_style_set_bg_color(&slider_main, lv_color_hex(palette::kBackground));
    lv_style_set_bg_opa(&slider_main, LV_OPA_COVER);
    lv_style_set_border_color(&slider_main, lv_color_hex(palette::kBorder));
    lv_style_set_border_width(&slider_main, 1);
    lv_style_set_radius(&slider_main, 4);

    lv_style_init(&slider_indicator);
    lv_style_set_bg_color(&slider_indicator, lv_color_hex(palette::kAccentPrimary));
    lv_style_set_bg_opa(&slider_indicator, LV_OPA_COVER);
    lv_style_set_radius(&slider_indicator, 4);

    lv_style_init(&slider_knob);
    lv_style_set_bg_color(&slider_knob, lv_color_hex(palette::kTextPrimary));
    lv_style_set_bg_opa(&slider_knob, LV_OPA_COVER);
    lv_style_set_pad_all(&slider_knob, 3);
    lv_style_set_radius(&slider_knob, 4);
}

ThemeStyles& theme() {
    if (!g_theme.initialized) {
        g_theme.init();
    }
    return g_theme;
}

void applyScreenStyle(lv_obj_t* obj) {
    if (!obj) return;
    theme();
    lv_obj_add_style(obj, &g_theme.screen, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void applyPanelStyle(lv_obj_t* obj, bool highlighted) {
    if (!obj) return;
    theme();
    lv_obj_add_style(obj, highlighted ? &g_theme.panel_highlight : &g_theme.panel, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void applyButtonStyle(lv_obj_t* obj, bool active, bool accent) {
    if (!obj) return;
    theme();
    lv_obj_add_style(obj, &g_theme.button, 0);
    if (active) {
        lv_obj_add_style(obj, &g_theme.button_active, 0);
    } else if (accent) {
        lv_obj_add_style(obj, &g_theme.button_accent, 0);
    }
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void applyPanicButtonStyle(lv_obj_t* obj) {
    if (!obj) return;
    theme();
    lv_obj_add_style(obj, &g_theme.button_panic, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t* createStatusBadge(lv_obj_t* parent, FeatureStatus status, int32_t x, int32_t y) {
    if (!parent) return nullptr;
    theme();
    lv_obj_t* box = lv_obj_create(parent);
    if (!box) return nullptr;
    lv_obj_remove_style_all(box);
    lv_obj_add_style(box, &g_theme.badge, 0);
    lv_obj_set_style_bg_color(box, lv_color_hex(featureStatusColor(status)), 0);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, 52, 18);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* lbl = lv_label_create(box);
    if (lbl) {
        lv_label_set_text_static(lbl, featureStatusLabel(status));
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        const uint32_t text_col = (status == FeatureStatus::Unavailable)
                                      ? palette::kTextPrimary
                                      : palette::kBackground;
        lv_obj_set_style_text_color(lbl, lv_color_hex(text_col), 0);
        lv_obj_center(lbl);
    }
    return box;
}

} // namespace smk::p4::ui
