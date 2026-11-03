#include "ui_blueprint.h"

namespace smk::p4::ui {

lv_obj_t* createLabel(lv_obj_t* parent, const char* text, int32_t x, int32_t y, int32_t w,
                      const lv_font_t* font, uint32_t color_hex) {
    if (!parent) return nullptr;
    lv_obj_t* lbl = lv_label_create(parent);
    if (!lbl) return nullptr;
    lv_obj_set_pos(lbl, x, y);
    lv_obj_set_size(lbl, w, lv_font_get_line_height(font) + 4);
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(color_hex), 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_MODE_CLIP);
    lv_label_set_text(lbl, text ? text : "");
    return lbl;
}

lv_obj_t* createActionButton(lv_obj_t* parent, const char* text, int32_t x, int32_t y,
                             int32_t w, int32_t h, lv_event_cb_t cb, void* user_data,
                             bool active, bool accent) {
    if (!parent) return nullptr;
    lv_obj_t* btn = lv_button_create(parent);
    if (!btn) return nullptr;
    applyButtonStyle(btn, active, accent);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);

    lv_obj_t* lbl = lv_label_create(btn);
    if (lbl) {
        lv_label_set_text(lbl, text ? text : "");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        const uint32_t tcol = (active || accent) ? palette::kBackground : palette::kTextPrimary;
        lv_obj_set_style_text_color(lbl, lv_color_hex(tcol), 0);
        lv_obj_center(lbl);
    }
    if (cb) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    }
    return btn;
}

bool UiBlueprint::begin(lv_obj_t* root_parent) {
    shutdown();
    theme().init();

    lv_obj_t* host = root_parent ? root_parent : lv_screen_active();
    if (!host) return false;

    root_ = lv_obj_create(host);
    if (!root_) return false;
    applyScreenStyle(root_);
    lv_obj_set_pos(root_, 0, 0);
    lv_obj_set_size(root_, kScreenWidth, kScreenHeight);

    content_ = lv_obj_create(root_);
    if (!content_) return false;
    applyScreenStyle(content_);
    lv_obj_set_pos(content_, 0, kContentTop);
    lv_obj_set_size(content_, kScreenWidth, kContentHeight);

    nav_.setOnNavigate(onNavigate, this);

    if (!status_strip_.build(root_, backend_, nav_)) return false;
    if (!nav_bar_.build(root_, nav_)) return false;
    if (!nav_.begin(content_, backend_)) return false;

    shown_rev_ = backend_.model().revision();
    shown_screen_ = nav_.currentScreen();
    status_strip_.refresh(backend_, nav_);
    nav_bar_.refresh(nav_);
    return true;
}

void UiBlueprint::shutdown() {
    nav_.shutdown();
    if (root_) {
        lv_obj_delete(root_);
        root_ = nullptr;
        content_ = nullptr;
    }
}

void UiBlueprint::service(uint32_t now_ms) {
    if (!root_) return;

    // Process any deferred navigation safely outside of LVGL widget callbacks
    if (nav_.hasPendingNavigation()) {
        nav_.processPendingNavigation();
    }

    backend_.service(now_ms);

    const uint32_t rev = backend_.model().revision();
    const ScreenId cur = nav_.currentScreen();
    if (rev != shown_rev_ || cur != shown_screen_) {
        shown_rev_ = rev;
        shown_screen_ = cur;
        status_strip_.refresh(backend_, nav_);
        nav_bar_.refresh(nav_);
        nav_.refreshActive();
    }
}

void UiBlueprint::onNavigate(void* user_data, ScreenId) {
    auto* self = static_cast<UiBlueprint*>(user_data);
    if (!self) return;
    self->status_strip_.refresh(self->backend_, self->nav_);
    self->nav_bar_.refresh(self->nav_);
}

} // namespace smk::p4::ui
