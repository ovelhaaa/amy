#include "ui_widgets.h"

namespace smk::p4::ui {

namespace {

struct ConfirmBtnCtx {
    ConfirmDialog* dialog = nullptr;
    void (*callback)(void* user_data) = nullptr;
    void* user_data = nullptr;
};

void onDialogAction(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    auto* ctx = static_cast<ConfirmBtnCtx*>(lv_event_get_user_data(e));
    if (!ctx || !ctx->dialog) return;
    auto cb = ctx->callback;
    void* ud = ctx->user_data;
    // Defer LVGL object destruction outside of the button's own event callback.
    ctx->dialog->close(true);
    if (cb) {
        cb(ud);
    }
}

} // namespace

bool ConfirmDialog::open(lv_obj_t* parent, const ConfirmDialogOptions& opts) {
    if (!parent) return false;
    close(false);

    backdrop_ = lv_obj_create(parent);
    if (!backdrop_) return false;
    lv_obj_remove_style_all(backdrop_);
    lv_obj_set_pos(backdrop_, 0, 0);
    lv_obj_set_size(backdrop_, kScreenWidth, kContentHeight);
    lv_obj_set_style_bg_opa(backdrop_, LV_OPA_70, 0);
    lv_obj_set_style_bg_color(backdrop_, lv_color_hex(0x05090D), 0);
    lv_obj_add_flag(backdrop_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(backdrop_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* card = lv_obj_create(backdrop_);
    if (!card) {
        close(false);
        return false;
    }
    applyPanelStyle(card, true);
    lv_obj_set_pos(card, 140, 74);
    lv_obj_set_size(card, 520, 224);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(opts.border_color), 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    createLabel(card, opts.title ? opts.title : "CONFIRMAR", 10, 8, 484,
                &lv_font_montserrat_14,
                opts.destructive ? palette::kErrorPanic : palette::kAccentPrimary);

    if (lv_obj_t* body_lbl = lv_label_create(card)) {
        lv_label_set_long_mode(body_lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_pos(body_lbl, 10, 40);
        lv_obj_set_size(body_lbl, 484, LV_SIZE_CONTENT);
        lv_obj_set_style_text_font(body_lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(body_lbl, lv_color_hex(palette::kTextPrimary), 0);
        lv_label_set_text(body_lbl, opts.body ? opts.body : "");
    }

    if (opts.detail && opts.detail[0]) {
        if (lv_obj_t* det_lbl = lv_label_create(card)) {
            lv_label_set_long_mode(det_lbl, LV_LABEL_LONG_WRAP);
            lv_obj_set_pos(det_lbl, 10, 96);
            lv_obj_set_size(det_lbl, 484, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(det_lbl, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(det_lbl, lv_color_hex(palette::kTextSecondary), 0);
            lv_label_set_text(det_lbl, opts.detail);
        }
    }

    lv_obj_t* cancel_btn = createActionButton(
        card, opts.cancel_label ? opts.cancel_label : "CANCELAR",
        10, 156, 220, 44, nullptr, nullptr, false, false);
    if (cancel_btn) {
        attachLvContext<ConfirmBtnCtx>(
            cancel_btn, ConfirmBtnCtx{this, opts.on_cancel, opts.user_data},
            onDialogAction, LV_EVENT_CLICKED);
    }

    lv_obj_t* confirm_btn = createActionButton(
        card, opts.confirm_label ? opts.confirm_label : "CONFIRMAR",
        274, 156, 220, 44, nullptr, nullptr, !opts.destructive, !opts.destructive);
    if (confirm_btn) {
        if (opts.destructive) {
            applyPanicButtonStyle(confirm_btn);
        }
        attachLvContext<ConfirmBtnCtx>(
            confirm_btn, ConfirmBtnCtx{this, opts.on_confirm, opts.user_data},
            onDialogAction, LV_EVENT_CLICKED);
    }

    return true;
}

void ConfirmDialog::close(bool async) {
    if (!backdrop_) return;
    lv_obj_t* target = backdrop_;
    backdrop_ = nullptr;
    if (async) {
        lv_obj_add_flag(target, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(target, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_delete_async(target);
    } else {
        lv_obj_delete(target);
    }
}

} // namespace smk::p4::ui
