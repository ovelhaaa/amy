#include "ui_widgets.h"
#include <cmath>
#include <cstdio>

namespace smk::p4::ui {

namespace {
struct SliderCtx {
    UiDemoBackend* backend   = nullptr;
    const char*    param_id  = nullptr;
    lv_obj_t*      val_label = nullptr;
};
} // namespace

lv_obj_t* createParameterSlider(lv_obj_t* parent, UiDemoBackend& backend, const char* param_id,
                                int32_t x, int32_t y, int32_t w, int32_t h) {
    if (!parent || !param_id) return nullptr;
    const ParameterDescriptor* desc = ParameterRegistry::findById(param_id);
    if (!desc) return nullptr;

    lv_obj_t* card = lv_obj_create(parent);
    if (!card) return nullptr;
    applyPanelStyle(card, false);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);

    const FeatureStatus status = backend.model().effectiveStatus(param_id);
    const bool enabled = backend.model().isParameterEnabled(param_id);

    createLabel(card, desc->name, 6, 4, w - 78, &lv_font_montserrat_14,
                enabled ? palette::kTextPrimary : palette::kTextSecondary);
    createStatusBadge(card, status, w - 74, 4);

    ParameterValue pv{};
    backend.model().getValue(param_id, pv);
    char vbuf[28];
    ParameterRegistry::formatValue(*desc, pv, vbuf, sizeof(vbuf));
    lv_obj_t* vlbl = createLabel(card, vbuf, 6, 24, w - 16, &lv_font_montserrat_14, palette::kAccentPrimary);

    lv_obj_t* slider = lv_slider_create(card);
    if (slider) {
        lv_obj_add_style(slider, &theme().slider_main, LV_PART_MAIN);
        lv_obj_add_style(slider, &theme().slider_indicator, LV_PART_INDICATOR);
        lv_obj_add_style(slider, &theme().slider_knob, LV_PART_KNOB);
        lv_obj_set_pos(slider, 8, h - 26);
        lv_obj_set_size(slider, w - 24, 14);
        lv_obj_set_ext_click_area(slider, 10);
        lv_slider_set_range(slider, 0, 100);

        const float span = (desc->maximum > desc->minimum) ? (desc->maximum - desc->minimum) : 1.0f;
        const float cur = backend.model().getFloat(param_id, desc->default_value);
        const int32_t pct = static_cast<int32_t>(std::lround(((cur - desc->minimum) / span) * 100.0f));
        lv_slider_set_value(slider, pct, LV_ANIM_OFF);

        if (!enabled) {
            lv_obj_add_state(slider, LV_STATE_DISABLED);
        }

        attachLvContext<SliderCtx>(
            slider, SliderCtx{&backend, desc->id, vlbl},
            [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
                auto* ctx = static_cast<SliderCtx*>(lv_event_get_user_data(e));
                if (!ctx || !ctx->backend || !ctx->param_id) return;
                const ParameterDescriptor* d = ParameterRegistry::findById(ctx->param_id);
                if (!d) return;
                auto* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
                const int32_t pct_val = lv_slider_get_value(target);
                const float norm = static_cast<float>(pct_val) / 100.0f;
                const float mapped = d->minimum + norm * (d->maximum - d->minimum);
                ctx->backend->model().setFloat(ctx->param_id, mapped);
                if (ctx->val_label) {
                    ParameterValue cur_pv{};
                    ctx->backend->model().getValue(ctx->param_id, cur_pv);
                    char buf[28];
                    ParameterRegistry::formatValue(*d, cur_pv, buf, sizeof(buf));
                    lv_label_set_text(ctx->val_label, buf);
                }
            },
            LV_EVENT_VALUE_CHANGED);
    }

    return card;
}

} // namespace smk::p4::ui
