#include "ui_widgets.h"
#include <cmath>
#include <cstdio>

namespace smk::p4::ui {

namespace {
struct SliderCtx {
    IUiBackend* backend   = nullptr;
    const char* param_id  = nullptr;
    lv_obj_t*   val_label = nullptr;
};
} // namespace

lv_obj_t* createParameterSlider(lv_obj_t* parent, IUiBackend& backend, const char* param_id,
                                int32_t x, int32_t y, int32_t w, int32_t h,
                                PresentationLevel level,
                                const char* label_override) {
    if (!parent || !param_id) return nullptr;
    const ParameterDescriptor* desc = ParameterRegistry::findById(param_id);
    if (!desc) return nullptr;

    lv_obj_t* card = lv_obj_create(parent);
    if (!card) return nullptr;
    applyPanelStyle(card, false);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);

    const FeatureStatus status = backend.parameterStatus(param_id);
    const bool enabled = backend.isParameterEditable(param_id);
    const char* title = (label_override && label_override[0] != '\0') ? label_override : desc->name;

    ParameterValue pv{};
    backend.model().getValue(param_id, pv);
    char vbuf[28];
    ParameterRegistry::formatValue(*desc, pv, vbuf, sizeof(vbuf));
    if (!backend.isDemoMode() && status != FeatureStatus::Available) std::snprintf(vbuf,sizeof(vbuf),"PLAN");

    lv_obj_t* vlbl = nullptr;
    if (level == PresentationLevel::StageMinimal) {
        // StageMinimal: cleaner header with title on left and prominent value on right, no technical badge clutter
        createLabel(card, title, 8, 4, w - 96, &lv_font_montserrat_14,
                    enabled ? palette::kTextPrimary : palette::kTextSecondary);
        const lv_font_t* val_font = (h >= 72) ? &lv_font_montserrat_20 : &lv_font_montserrat_14;
        vlbl = createLabel(card, vbuf, w - 88, (h >= 72) ? 2 : 4, 76, val_font, palette::kAccentPrimary);
        if (vlbl) {
            lv_obj_set_style_text_align(vlbl, LV_TEXT_ALIGN_RIGHT, 0);
        }
    } else {
        createLabel(card, title, 6, 4, w - 64, &lv_font_montserrat_14,
                    enabled ? palette::kTextPrimary : palette::kTextSecondary);
        createStatusBadge(card, status, w - 62, 4);
        vlbl = createLabel(card, vbuf, 6, 24, w - 16, &lv_font_montserrat_14, palette::kAccentPrimary);
    }

    lv_obj_t* slider = lv_slider_create(card);
    if (slider) {
        lv_obj_add_style(slider, &theme().slider_main, LV_PART_MAIN);
        lv_obj_add_style(slider, &theme().slider_indicator, LV_PART_INDICATOR);
        lv_obj_add_style(slider, &theme().slider_knob, LV_PART_KNOB);

        const int32_t bar_h = (level == PresentationLevel::StageMinimal && h >= 68) ? 18 : 14;
        const int32_t bar_y = (level == PresentationLevel::StageMinimal && h >= 68) ? (h - 32) : (h - 26);
        lv_obj_set_pos(slider, 10, bar_y);
        lv_obj_set_size(slider, w - 28, bar_h);
        lv_obj_set_ext_click_area(slider, 14);
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
                const float span_v = (d->maximum > d->minimum) ? (d->maximum - d->minimum) : 1.0f;
                const float norm = static_cast<float>(pct_val) / 100.0f;
                const float mapped = d->minimum + norm * span_v;
                const UiCommandResult res = ctx->backend->requestParameterFloat(ctx->param_id, mapped);
                if (!res.accepted()) {
                    const float confirmed = ctx->backend->model().getFloat(ctx->param_id, d->default_value);
                    const int32_t conf_pct = static_cast<int32_t>(std::lround(((confirmed - d->minimum) / span_v) * 100.0f));
                    lv_slider_set_value(target, conf_pct, LV_ANIM_OFF);
                }
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

    if (slider) {
        attachLvContext<SliderCtx>(slider, SliderCtx{&backend, desc->id, vlbl}, [](lv_event_t* e) {
            auto* ctx=static_cast<SliderCtx*>(lv_event_get_user_data(e));
            auto* obj=static_cast<lv_obj_t*>(lv_event_get_target(e));
            const auto* d=ParameterRegistry::findById(ctx->param_id);
            ParameterValue value{}; ctx->backend->model().getValue(ctx->param_id,value);
            char text[28]; ParameterRegistry::formatValue(*d,value,text,sizeof(text));
            if(!ctx->backend->isDemoMode() && ctx->backend->parameterStatus(ctx->param_id)!=FeatureStatus::Available) std::snprintf(text,sizeof(text),"PLAN");
            if(ctx->val_label) lv_label_set_text(ctx->val_label,text);
            if(!lv_obj_has_state(obj,LV_STATE_PRESSED)) {
                const float span=d->maximum-d->minimum;
                lv_slider_set_value(obj,span>0?static_cast<int32_t>(std::lround((value.asFloat()-d->minimum)*100/span)):0,LV_ANIM_OFF);
            }
            if(ctx->backend->isParameterEditable(ctx->param_id)) lv_obj_remove_state(obj,LV_STATE_DISABLED);
            else lv_obj_add_state(obj,LV_STATE_DISABLED);
        }, LV_EVENT_REFRESH);
    }
    return card;
}

} // namespace smk::p4::ui
