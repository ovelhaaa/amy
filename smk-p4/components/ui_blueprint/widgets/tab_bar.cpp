#include "ui_widgets.h"
#include <cstring>

namespace smk::p4::ui {

namespace {

struct AreaBtnCtx {
    UiNavigationManager*              nav   = nullptr;
    BottomNavBar::AreaClickCallback cb    = nullptr;
    void*                             user  = nullptr;
    UiArea                            area  = UiArea::Tocar;
};

struct SubTabCtx {
    UiNavigationManager* nav    = nullptr;
    ScreenId             screen = ScreenId::Performance;
    uint8_t              tab    = 0;
};

} // namespace

bool BottomNavBar::build(lv_obj_t* parent, UiNavigationManager& nav,
                         AreaClickCallback on_area_click, void* user_data) {
    if (!parent) return false;
    on_area_click_ = on_area_click;
    on_area_user_ = user_data;

    root_ = lv_obj_create(parent);
    if (!root_) return false;
    applyPanelStyle(root_, false);
    lv_obj_set_pos(root_, 0, kContentBottom);
    lv_obj_set_size(root_, kScreenWidth, kNavBarHeight);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);

    // 5 fixed permanent musical areas: TOCAR | TIMBRES | RITMO | FX | MAIS
    for (uint8_t i = 0; i < 5; ++i) {
        const int32_t bx = 8 + static_cast<int32_t>(i) * 158;
        const UiArea area = static_cast<UiArea>(i);
        lv_obj_t* btn = lv_button_create(root_);
        if (!btn) continue;
        lv_obj_remove_style_all(btn);
        lv_obj_add_style(btn, &theme().tab_button, 0);
        lv_obj_add_style(btn, &theme().tab_button_active, LV_STATE_CHECKED);
        lv_obj_set_pos(btn, bx, 4);
        lv_obj_set_size(btn, 150, 40);
        lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lbl = lv_label_create(btn);
        if (lbl) {
            lv_label_set_text_static(lbl, areaLabel(area));
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
            lv_obj_center(lbl);
        }
        buttons_[i] = btn;
        labels_[i] = lbl;

        attachLvContext<AreaBtnCtx>(
            btn, AreaBtnCtx{&nav, on_area_click_, on_area_user_, area},
            [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                auto* ctx = static_cast<AreaBtnCtx*>(lv_event_get_user_data(e));
                if (!ctx || !ctx->nav) return;
                if (ctx->cb) {
                    ctx->cb(ctx->user, ctx->area);
                } else {
                    ctx->nav->navigateToArea(ctx->area);
                }
            },
            LV_EVENT_CLICKED);
    }

    refresh(nav);
    return true;
}

void BottomNavBar::refresh(UiNavigationManager& nav) {
    if (!root_) return;
    const UiArea active = nav.currentArea();

    for (uint8_t i = 0; i < 5; ++i) {
        if (!buttons_[i]) continue;
        const bool want_checked = (static_cast<uint8_t>(active) == i);
        const bool is_checked = lv_obj_has_state(buttons_[i], LV_STATE_CHECKED);
        if (want_checked && !is_checked) {
            lv_obj_add_state(buttons_[i], LV_STATE_CHECKED);
        } else if (!want_checked && is_checked) {
            lv_obj_remove_state(buttons_[i], LV_STATE_CHECKED);
        }
    }
}

lv_obj_t* createSubTabBar(lv_obj_t* parent, const char* const* tab_names, uint8_t tab_count,
                          uint8_t active_tab, ScreenId screen_id, UiNavigationManager& nav,
                          int32_t x, int32_t y, int32_t w, int32_t h) {
    if (!parent || !tab_names || tab_count == 0) return nullptr;
    lv_obj_t* bar = lv_obj_create(parent);
    if (!bar) return nullptr;
    applyScreenStyle(bar);
    lv_obj_set_pos(bar, x, y);
    lv_obj_set_size(bar, w, h);

    const int32_t gap = 4;
    const int32_t btn_w = (w - gap * (tab_count - 1)) / tab_count;
    for (uint8_t i = 0; i < tab_count; ++i) {
        lv_obj_t* btn = lv_button_create(bar);
        if (!btn) continue;
        lv_obj_remove_style_all(btn);
        lv_obj_add_style(btn, &theme().tab_button, 0);
        if (i == active_tab) {
            lv_obj_add_style(btn, &theme().tab_button_active, 0);
        }
        lv_obj_set_pos(btn, i * (btn_w + gap), 0);
        lv_obj_set_size(btn, btn_w, h);
        lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lbl = lv_label_create(btn);
        if (lbl) {
            lv_label_set_text(lbl, tab_names[i]);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(
                lbl, lv_color_hex(i == active_tab ? palette::kAccentPrimary : palette::kTextPrimary), 0);
            lv_obj_center(lbl);
        }

        attachLvContext<SubTabCtx>(
            btn, SubTabCtx{&nav, screen_id, i},
            [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                auto* ctx = static_cast<SubTabCtx*>(lv_event_get_user_data(e));
                if (!ctx || !ctx->nav) return;
                ctx->nav->navigateTo(ctx->screen, ctx->tab);
            },
            LV_EVENT_CLICKED);
    }
    return bar;
}

} // namespace smk::p4::ui
