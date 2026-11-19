#include "ui_widgets.h"
#include <cstdio>
#include <cstring>

namespace smk::p4::ui {

namespace {
struct CardActionCtx {
    IUiBackend* backend      = nullptr;
    const char* param_id     = nullptr;
    lv_obj_t*   title_label  = nullptr;
    lv_obj_t*   val_label    = nullptr;
    lv_obj_t*   btn_a        = nullptr;
    lv_obj_t*   btn_b        = nullptr;
    int32_t     delta        = 0;
    bool        last_editable = true;
    bool        has_editable  = false;
};

void setLabelTextIfChanged(lv_obj_t* lbl, const char* text) {
    if (!lbl || !text) return;
    if (std::strcmp(lv_label_get_text(lbl), text) != 0) {
        lv_label_set_text(lbl, text);
    }
}

void setDisabledState(lv_obj_t* obj, bool disabled) {
    if (!obj) return;
    const bool has = lv_obj_has_state(obj, LV_STATE_DISABLED);
    if (disabled && !has) lv_obj_add_state(obj, LV_STATE_DISABLED);
    else if (!disabled && has) lv_obj_remove_state(obj, LV_STATE_DISABLED);
}

void setCheckedState(lv_obj_t* obj, bool checked) {
    if (!obj) return;
    const bool has = lv_obj_has_state(obj, LV_STATE_CHECKED);
    if (checked && !has) lv_obj_add_state(obj, LV_STATE_CHECKED);
    else if (!checked && has) lv_obj_remove_state(obj, LV_STATE_CHECKED);
}
} // namespace

lv_obj_t* createParameterCard(lv_obj_t* parent, IUiBackend& backend, const char* param_id,
                              int32_t x, int32_t y, int32_t w, int32_t h,
                              PresentationLevel level,
                              const char* label_override) {
    if (!parent || !param_id) return nullptr;
    const ParameterDescriptor* desc = ParameterRegistry::findById(param_id);
    if (!desc) return nullptr;

    // For Float and continuous Int with wide range, delegate to slider card
    if (desc->type == ParameterValueType::Float ||
        (desc->type == ParameterValueType::Int && (desc->maximum - desc->minimum) > 24.0f &&
         !(desc->capabilities & kCapReadOnly))) {
        return createParameterSlider(parent, backend, param_id, x, y, w, h, level, label_override);
    }

    lv_obj_t* card = lv_obj_create(parent);
    if (!card) return nullptr;
    applyPanelStyle(card, false);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);

    const FeatureStatus status = backend.parameterStatus(param_id);
    const bool enabled = backend.isParameterEditable(param_id);
    const char* title = (label_override && label_override[0] != '\0') ? label_override : desc->name;

    const int32_t title_w = (level == PresentationLevel::BlueprintVerbose) ? (w - 64) : (w - 16);
    lv_obj_t* title_lbl = createLabel(card, title, 6, 4, title_w, &lv_font_montserrat_14,
                                      enabled ? palette::kTextPrimary : palette::kTextSecondary);
    if (level == PresentationLevel::BlueprintVerbose) {
        createStatusBadge(card, status, w - 62, 4);
    }

    ParameterValue pv{};
    backend.model().getValue(param_id, pv);
    char vbuf[32];
    ParameterRegistry::formatValue(*desc, pv, vbuf, sizeof(vbuf));

    if (desc->type == ParameterValueType::Bool) {
        lv_obj_t* btn = lv_button_create(card);
        if (btn) {
            lv_obj_remove_style_all(btn);
            lv_obj_add_style(btn, &theme().button, 0);
            lv_obj_add_style(btn, &theme().button_active, LV_STATE_CHECKED);
            lv_obj_set_style_opa(btn, LV_OPA_50, LV_STATE_DISABLED);
            lv_obj_set_pos(btn, 6, h - 44);
            lv_obj_set_size(btn, w - 24, 36);
            lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
            if (pv.asBool()) lv_obj_add_state(btn, LV_STATE_CHECKED);
            if (!enabled) lv_obj_add_state(btn, LV_STATE_DISABLED);

            lv_obj_t* lbl = lv_label_create(btn);
            if (lbl) {
                lv_label_set_text(lbl, vbuf);
                lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
                lv_obj_center(lbl);
            }

            attachLvContext<CardActionCtx>(
                card, CardActionCtx{&backend, desc->id, title_lbl, lbl, btn, nullptr, 0, enabled, true},
                [](lv_event_t* e) {
                    auto* ctx = static_cast<CardActionCtx*>(lv_event_get_user_data(e));
                    if (!ctx || !ctx->backend || !ctx->param_id) return;
                    const bool ed = ctx->backend->isParameterEditable(ctx->param_id);
                    const bool on = ctx->backend->model().getBool(ctx->param_id, false);
                    setDisabledState(ctx->btn_a, !ed);
                    setCheckedState(ctx->btn_a, on);
                    if (!ctx->has_editable || ctx->last_editable != ed) {
                        if (ctx->title_label) {
                            lv_obj_set_style_text_color(
                                ctx->title_label, lv_color_hex(ed ? palette::kTextPrimary : palette::kTextSecondary), 0);
                        }
                        ctx->last_editable = ed;
                        ctx->has_editable = true;
                    }
                    setLabelTextIfChanged(ctx->val_label, on ? "ON" : "OFF");
                },
                LV_EVENT_REFRESH);

            attachLvContext<CardActionCtx>(
                btn, CardActionCtx{&backend, desc->id, title_lbl, lbl, btn, nullptr, 1},
                [](lv_event_t* e) {
                    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                    auto* ctx = static_cast<CardActionCtx*>(lv_event_get_user_data(e));
                    if (!ctx || !ctx->backend || !ctx->param_id) return;
                    if (!ctx->backend->isParameterEditable(ctx->param_id)) return;
                    const bool next = !ctx->backend->model().getBool(ctx->param_id, false);
                    ctx->backend->requestParameterBool(ctx->param_id, next);
                    const bool applied = ctx->backend->model().getBool(ctx->param_id, false);
                    setCheckedState(ctx->btn_a, applied);
                    setLabelTextIfChanged(ctx->val_label, applied ? "ON" : "OFF");
                },
                LV_EVENT_CLICKED);
        }
    } else if (desc->capabilities & kCapReadOnly) {
        createLabel(card, vbuf, 8, h - 34, w - 24, &lv_font_montserrat_20, palette::kTextSecondary);
    } else {
        // Stepper buttons [-] [VALUE] [+] for Enum and discrete Int
        const int32_t btn_w = (w < 140) ? 28 : ((level == PresentationLevel::StageStacked && w >= 200) ? 44 : 38);
        const lv_font_t* val_font = (level == PresentationLevel::StageStacked && h >= 110)
                                        ? &lv_font_montserrat_20 : &lv_font_montserrat_14;
        const int32_t val_y = (val_font == &lv_font_montserrat_20) ? (h - 40) : (h - 36);
        lv_obj_t* vlbl = createLabel(card, vbuf, btn_w + 8, val_y, w - (btn_w * 2 + 24),
                                     val_font, enabled ? palette::kAccentPrimary : palette::kTextSecondary);
        if (vlbl && level == PresentationLevel::StageStacked) {
            lv_obj_set_style_text_align(vlbl, LV_TEXT_ALIGN_CENTER, 0);
        }
        lv_obj_t* dec_btn = createActionButton(card, "-", 4, h - 44, btn_w, 36, nullptr, nullptr);
        lv_obj_t* inc_btn = createActionButton(card, "+", w - btn_w - 14, h - 44, btn_w, 36, nullptr, nullptr);
        setDisabledState(dec_btn, !enabled);
        setDisabledState(inc_btn, !enabled);
        if (dec_btn) lv_obj_set_style_opa(dec_btn, LV_OPA_50, LV_STATE_DISABLED);
        if (inc_btn) lv_obj_set_style_opa(inc_btn, LV_OPA_50, LV_STATE_DISABLED);

        attachLvContext<CardActionCtx>(
            card, CardActionCtx{&backend, desc->id, title_lbl, vlbl, dec_btn, inc_btn, 0, enabled, true},
            [](lv_event_t* e) {
                auto* ctx = static_cast<CardActionCtx*>(lv_event_get_user_data(e));
                if (!ctx || !ctx->backend || !ctx->param_id) return;
                const auto* d = ParameterRegistry::findById(ctx->param_id);
                if (!d) return;
                const bool ed = ctx->backend->isParameterEditable(ctx->param_id);
                setDisabledState(ctx->btn_a, !ed);
                setDisabledState(ctx->btn_b, !ed);
                if (!ctx->has_editable || ctx->last_editable != ed) {
                    if (ctx->title_label) {
                        lv_obj_set_style_text_color(
                            ctx->title_label, lv_color_hex(ed ? palette::kTextPrimary : palette::kTextSecondary), 0);
                    }
                    if (ctx->val_label) {
                        lv_obj_set_style_text_color(
                            ctx->val_label, lv_color_hex(ed ? palette::kAccentPrimary : palette::kTextSecondary), 0);
                    }
                    ctx->last_editable = ed;
                    ctx->has_editable = true;
                }
                if (ctx->val_label) {
                    ParameterValue v{};
                    ctx->backend->model().getValue(ctx->param_id, v);
                    char text[32];
                    ParameterRegistry::formatValue(*d, v, text, sizeof(text));
                    setLabelTextIfChanged(ctx->val_label, text);
                }
            },
            LV_EVENT_REFRESH);

        auto step_cb = [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
            auto* ctx = static_cast<CardActionCtx*>(lv_event_get_user_data(e));
            if (!ctx || !ctx->backend || !ctx->param_id) return;
            if (!ctx->backend->isParameterEditable(ctx->param_id)) return;
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
            for (int32_t tries = 0; tries <= max_v - min_v && !ctx->backend->isParameterValueSupported(ctx->param_id, cur); ++tries) {
                cur += ctx->delta;
                if (cur < min_v) cur = max_v;
                if (cur > max_v) cur = min_v;
            }
            ctx->backend->requestParameterInt(ctx->param_id, cur);
            if (ctx->val_label) {
                ParameterValue cur_pv{};
                ctx->backend->model().getValue(ctx->param_id, cur_pv);
                char buf[32];
                ParameterRegistry::formatValue(*d, cur_pv, buf, sizeof(buf));
                setLabelTextIfChanged(ctx->val_label, buf);
            }
        };
        if (dec_btn) attachLvContext<CardActionCtx>(dec_btn, CardActionCtx{&backend, desc->id, title_lbl, vlbl, dec_btn, inc_btn, -1}, step_cb, LV_EVENT_CLICKED);
        if (inc_btn) attachLvContext<CardActionCtx>(inc_btn, CardActionCtx{&backend, desc->id, title_lbl, vlbl, dec_btn, inc_btn, +1}, step_cb, LV_EVENT_CLICKED);
    }

    return card;
}

} // namespace smk::p4::ui
