#include "ui_widgets.h"
#include <cstdio>

namespace smk::p4::ui {

namespace {
struct CardActionCtx {
    UiDemoBackend* backend   = nullptr;
    const char*    param_id  = nullptr;
    lv_obj_t*      val_label = nullptr;
    int32_t        delta     = 0;
};
} // namespace

lv_obj_t* createParameterCard(lv_obj_t* parent, UiDemoBackend& backend, const char* param_id,
                              int32_t x, int32_t y, int32_t w, int32_t h) {
    if (!parent || !param_id) return nullptr;
    const ParameterDescriptor* desc = ParameterRegistry::findById(param_id);
    if (!desc) return nullptr;

    // For Float and continuous Int with wide range, delegate to slider card
    if (desc->type == ParameterValueType::Float ||
        (desc->type == ParameterValueType::Int && (desc->maximum - desc->minimum) > 24.0f &&
         !(desc->capabilities & kCapReadOnly))) {
        return createParameterSlider(parent, backend, param_id, x, y, w, h);
    }

    lv_obj_t* card = lv_obj_create(parent);
    if (!card) return nullptr;
    applyPanelStyle(card, false);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);

    const FeatureStatus status = backend.model().effectiveStatus(param_id);
    const bool enabled = backend.model().isParameterEnabled(param_id);

    createLabel(card, desc->name, 6, 4, w - 64, &lv_font_montserrat_14,
                enabled ? palette::kTextPrimary : palette::kTextSecondary);
    createStatusBadge(card, status, w - 62, 4);

    ParameterValue pv{};
    backend.model().getValue(param_id, pv);
    char vbuf[32];
    ParameterRegistry::formatValue(*desc, pv, vbuf, sizeof(vbuf));

    if (desc->type == ParameterValueType::Bool) {
        lv_obj_t* btn = createActionButton(card, vbuf, 6, h - 42, w - 24, 34, nullptr, nullptr,
                                           pv.asBool(), false);
        if (!enabled && btn) lv_obj_add_state(btn, LV_STATE_DISABLED);
        if (btn) {
            lv_obj_t* lbl = lv_obj_get_child(btn, 0);
            attachLvContext<CardActionCtx>(
                btn, CardActionCtx{&backend, desc->id, lbl, 1},
                [](lv_event_t* e) {
                    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                    auto* ctx = static_cast<CardActionCtx*>(lv_event_get_user_data(e));
                    if (!ctx || !ctx->backend || !ctx->param_id) return;
                    const bool next = !ctx->backend->model().getBool(ctx->param_id, false);
                    ctx->backend->model().setBool(ctx->param_id, next);
                    if (ctx->val_label) {
                        lv_label_set_text(ctx->val_label, next ? "ON" : "OFF");
                    }
                },
                LV_EVENT_CLICKED);
        }
    } else if (desc->capabilities & kCapReadOnly) {
        createLabel(card, vbuf, 8, h - 34, w - 24, &lv_font_montserrat_20, palette::kTextSecondary);
    } else {
        // Stepper buttons [-] [VALUE] [+] for Enum and discrete Int
        const int32_t btn_w = (w < 140) ? 26 : 36;
        lv_obj_t* vlbl = createLabel(card, vbuf, btn_w + 8, h - 34, w - (btn_w * 2 + 24), &lv_font_montserrat_14, palette::kAccentPrimary);
        lv_obj_t* dec_btn = createActionButton(card, "-", 4, h - 42, btn_w, 34, nullptr, nullptr);
        lv_obj_t* inc_btn = createActionButton(card, "+", w - btn_w - 14, h - 42, btn_w, 34, nullptr, nullptr);
        if (!enabled) {
            if (dec_btn) lv_obj_add_state(dec_btn, LV_STATE_DISABLED);
            if (inc_btn) lv_obj_add_state(inc_btn, LV_STATE_DISABLED);
        }
        auto step_cb = [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
            auto* ctx = static_cast<CardActionCtx*>(lv_event_get_user_data(e));
            if (!ctx || !ctx->backend || !ctx->param_id) return;
            const ParameterDescriptor* d = ParameterRegistry::findById(ctx->param_id);
            if (!d) return;
            int32_t cur = ctx->backend->model().getInt(ctx->param_id, 0);
            const int32_t min_v = static_cast<int32_t>(d->minimum);
            const int32_t max_v = (d->type == ParameterValueType::Enum && d->enum_count > 0)
                                      ? static_cast<int32_t>(d->enum_count - 1)
                                      : static_cast<int32_t>(d->maximum);
            cur += ctx->delta;
            if (cur < min_v) cur = max_v;
            if (cur > max_v) cur = min_v;
            ctx->backend->model().setInt(ctx->param_id, cur);
            if (ctx->val_label) {
                ParameterValue cur_pv{};
                ctx->backend->model().getValue(ctx->param_id, cur_pv);
                char buf[32];
                ParameterRegistry::formatValue(*d, cur_pv, buf, sizeof(buf));
                lv_label_set_text(ctx->val_label, buf);
            }
        };
        if (dec_btn) attachLvContext<CardActionCtx>(dec_btn, CardActionCtx{&backend, desc->id, vlbl, -1}, step_cb, LV_EVENT_CLICKED);
        if (inc_btn) attachLvContext<CardActionCtx>(inc_btn, CardActionCtx{&backend, desc->id, vlbl, +1}, step_cb, LV_EVENT_CLICKED);
    }

    return card;
}

} // namespace smk::p4::ui
