#include "ui_widgets.h"

namespace smk::p4::ui {

namespace {

// Navigation banks (always <= 6 screen destinations visible at once + 1 bank cycle button)
// Bank 0 (Primary): PERFORMANCE | SOUND | LAYERS | FX | SEQ | HARMONY
// Bank 1 (Tools):   PERFORMANCE | PATCHES | MIXER | MOD | ARP | DRUMS
// Bank 2 (System):  PERFORMANCE | PATCHES | MIDI | SETTINGS | DIAG | HARMONY
constexpr ScreenId kBankScreens[3][6] = {
    {ScreenId::Performance, ScreenId::SoundEditor, ScreenId::Layers,
     ScreenId::Effects,     ScreenId::Sequencer,   ScreenId::Harmony},
    {ScreenId::Performance, ScreenId::PatchBrowser,ScreenId::Mixer,
     ScreenId::Modulation,  ScreenId::Arpeggiator, ScreenId::Drums},
    {ScreenId::Performance, ScreenId::PatchBrowser,ScreenId::MidiSettings,
     ScreenId::GlobalSettings, ScreenId::Diagnostics, ScreenId::Harmony}
};

constexpr const char* kBankButtonLabels[3] = {
    "BANK: MAIN >",
    "BANK: TOOLS >",
    "BANK: SYS >"
};

struct NavBtnCtx {
    UiNavigationManager* nav   = nullptr;
    BottomNavBar*        bar   = nullptr;
    uint8_t              slot  = 0;
    bool                 is_bank_toggle = false;
};

struct SubTabCtx {
    UiNavigationManager* nav    = nullptr;
    ScreenId             screen = ScreenId::Performance;
    uint8_t              tab    = 0;
};

} // namespace

bool BottomNavBar::build(lv_obj_t* parent, UiNavigationManager& nav) {
    if (!parent) return false;
    root_ = lv_obj_create(parent);
    if (!root_) return false;
    applyPanelStyle(root_, false);
    lv_obj_set_pos(root_, 0, kContentBottom);
    lv_obj_set_size(root_, kScreenWidth, kNavBarHeight);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);

    // 6 destination buttons (each 108px wide) + 1 bank selector button (122px wide)
    for (uint8_t i = 0; i < 6; ++i) {
        const int32_t bx = 6 + i * 110;
        lv_obj_t* btn = lv_button_create(root_);
        if (!btn) continue;
        lv_obj_remove_style_all(btn);
        lv_obj_add_style(btn, &theme().tab_button, 0);
        lv_obj_set_pos(btn, bx, 4);
        lv_obj_set_size(btn, 106, 38);
        lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lbl = lv_label_create(btn);
        if (lbl) {
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
            lv_obj_center(lbl);
        }
        buttons_[i] = btn;
        labels_[i] = lbl;

        attachLvContext<NavBtnCtx>(
            btn, NavBtnCtx{&nav, this, i, false},
            [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                auto* ctx = static_cast<NavBtnCtx*>(lv_event_get_user_data(e));
                if (!ctx || !ctx->nav) return;
                const uint8_t bank = ctx->nav->navBank() % 3;
                const ScreenId target = kBankScreens[bank][ctx->slot];
                ctx->nav->navigateTo(target);
            },
            LV_EVENT_CLICKED);
    }

    bank_btn_ = lv_button_create(root_);
    if (bank_btn_) {
        applyButtonStyle(bank_btn_, false, true);
        lv_obj_set_pos(bank_btn_, 666, 4);
        lv_obj_set_size(bank_btn_, 128, 38);
        bank_lbl_ = lv_label_create(bank_btn_);
        if (bank_lbl_) {
            lv_obj_set_style_text_font(bank_lbl_, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(bank_lbl_, lv_color_hex(palette::kBackground), 0);
            lv_obj_center(bank_lbl_);
        }
        attachLvContext<NavBtnCtx>(
            bank_btn_, NavBtnCtx{&nav, this, 0, true},
            [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                auto* ctx = static_cast<NavBtnCtx*>(lv_event_get_user_data(e));
                if (!ctx || !ctx->nav || !ctx->bar) return;
                ctx->nav->cycleNavBank();
                ctx->bar->refresh(*ctx->nav);
            },
            LV_EVENT_CLICKED);
    }

    refresh(nav);
    return true;
}

void BottomNavBar::refresh(UiNavigationManager& nav) {
    if (!root_) return;
    const uint8_t bank = nav.navBank() % 3;
    const ScreenId active = nav.currentScreen();

    for (uint8_t i = 0; i < 6; ++i) {
        if (!buttons_[i] || !labels_[i]) continue;
        const ScreenId dest = kBankScreens[bank][i];
        lv_label_set_text_static(labels_[i], screenShortName(dest));

        lv_obj_remove_style(buttons_[i], &theme().tab_button_active, 0);
        if (dest == active) {
            lv_obj_add_style(buttons_[i], &theme().tab_button_active, 0);
            lv_obj_set_style_text_color(labels_[i], lv_color_hex(palette::kAccentPrimary), 0);
        } else {
            lv_obj_set_style_text_color(labels_[i], lv_color_hex(palette::kTextPrimary), 0);
        }
    }

    if (bank_lbl_) {
        lv_label_set_text_static(bank_lbl_, kBankButtonLabels[bank]);
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
