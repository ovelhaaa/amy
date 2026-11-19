#include "ui_widgets.h"
#include <cmath>
#include <cstdio>

namespace smk::p4::ui {

namespace {
float sliderValue(const ParameterDescriptor& d,float norm){
    if(d.scale==ParameterScale::Logarithmic && d.minimum>0 && d.maximum>d.minimum)
        return d.minimum*std::pow(d.maximum/d.minimum,norm);
    return d.minimum+norm*(d.maximum-d.minimum);
}
int32_t sliderPercent(const ParameterDescriptor& d,float value){
    if(d.scale==ParameterScale::Logarithmic && d.minimum>0 && d.maximum>d.minimum && value>0)
        return int32_t(std::lround(100*std::log(value/d.minimum)/std::log(d.maximum/d.minimum)));
    return d.maximum>d.minimum?int32_t(std::lround(100*(value-d.minimum)/(d.maximum-d.minimum))):0;
}
struct SliderCtx {
    IUiBackend* backend   = nullptr;
    const char* param_id  = nullptr;
    lv_obj_t*   val_label = nullptr;
    lv_obj_t*   badge_box = nullptr;
    uint32_t    last_send_tick_ms      = 0;
    bool        has_sent               = false;
    bool        pending_release_commit = false;
};

ParameterValue makeSliderParamValue(const ParameterDescriptor& d, float mapped) {
    if (d.type == ParameterValueType::Int) return ParameterValue::makeInt(static_cast<int32_t>(std::lround(mapped)));
    if (d.type == ParameterValueType::Enum) return ParameterValue::makeEnum(static_cast<int32_t>(std::lround(mapped)));
    if (d.type == ParameterValueType::Bool) return ParameterValue::makeBool(mapped >= 0.5f);
    return ParameterValue::makeFloat(mapped);
}

void commitSliderValue(SliderCtx& ctx, lv_obj_t* target) {
    const ParameterDescriptor* d = ParameterRegistry::findById(ctx.param_id);
    if (!d) return;
    const int32_t pct_val = lv_slider_get_value(target);
    const float norm = static_cast<float>(pct_val) / 100.0f;
    const float mapped = sliderValue(*d, norm);
    const UiCommandResult res = ctx.backend->requestParameterFloat(ctx.param_id, mapped);
    ctx.last_send_tick_ms = lv_tick_get();
    ctx.has_sent = true;
    ctx.pending_release_commit = false;
    if (!res.accepted()) {
        const float confirmed = ctx.backend->model().getFloat(ctx.param_id, d->default_value);
        lv_slider_set_value(target, sliderPercent(*d, confirmed), LV_ANIM_OFF);
    }
    if (ctx.val_label) {
        ParameterValue shown{};
        if (res.accepted() && res.state == UiCommandState::Requested) {
            shown = makeSliderParamValue(*d, mapped);
        } else {
            ctx.backend->model().getValue(ctx.param_id, shown);
        }
        char buf[28];
        ParameterRegistry::formatValue(*d, shown, buf, sizeof(buf));
        lv_label_set_text(ctx.val_label, buf);
    }
}
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
    if (!backend.isDemoMode() && status != FeatureStatus::Available) std::snprintf(vbuf,sizeof(vbuf),"%s",status==FeatureStatus::Unavailable?"N/A":"PLAN");

    lv_obj_t* vlbl = nullptr;
    lv_obj_t* badge_box = nullptr;
    if (level == PresentationLevel::StageMinimal) {
        // StageMinimal: cleaner header with title on left and prominent value on right, no technical badge clutter
        createLabel(card, title, 8, 4, w - 96, &lv_font_montserrat_14,
                    enabled ? palette::kTextPrimary : palette::kTextSecondary);
        const lv_font_t* val_font = (h >= 72) ? &lv_font_montserrat_20 : &lv_font_montserrat_14;
        vlbl = createLabel(card, vbuf, w - 88, (h >= 72) ? 2 : 4, 76, val_font, palette::kAccentPrimary);
        if (vlbl) {
            lv_obj_set_style_text_align(vlbl, LV_TEXT_ALIGN_RIGHT, 0);
        }
    } else if(level==PresentationLevel::StageStacked) {
        createLabel(card,title,6,4,w-16,&lv_font_montserrat_14,
                    enabled?palette::kTextPrimary:palette::kTextSecondary);
        vlbl=createLabel(card,vbuf,6,24,w-16,&lv_font_montserrat_14,palette::kAccentPrimary);
    } else {
        createLabel(card, title, 6, 4, w - 64, &lv_font_montserrat_14,
                    enabled ? palette::kTextPrimary : palette::kTextSecondary);
        badge_box = createStatusBadge(card, status, w - 62, 4);
        vlbl = createLabel(card, vbuf, 6, 24, w - 16, &lv_font_montserrat_14, palette::kAccentPrimary);
    }

    lv_obj_t* slider = lv_slider_create(card);
    if (slider) {
        lv_obj_add_style(slider, &theme().slider_main, LV_PART_MAIN);
        lv_obj_add_style(slider, &theme().slider_indicator, LV_PART_INDICATOR);
        lv_obj_add_style(slider, &theme().slider_knob, LV_PART_KNOB);

        const bool stage=level!=PresentationLevel::BlueprintVerbose && h>=68;
        const int32_t bar_h = stage ? 18 : 14;
        const int32_t bar_y = stage ? (h - 32) : (h - 26);
        lv_obj_set_pos(slider, 10, bar_y);
        lv_obj_set_size(slider, w - 28, bar_h);
        lv_obj_set_ext_click_area(slider, 14);
        lv_slider_set_range(slider, 0, 100);

        const float cur = backend.model().getFloat(param_id, desc->default_value);
        const int32_t pct = sliderPercent(*desc,cur);
        lv_slider_set_value(slider, pct, LV_ANIM_OFF);

        if (!enabled) {
            lv_obj_add_state(slider, LV_STATE_DISABLED);
        }

        SliderCtx* ctx = attachLvContext<SliderCtx>(
            slider, SliderCtx{&backend, desc->id, vlbl, badge_box},
            [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
                auto* sctx = static_cast<SliderCtx*>(lv_event_get_user_data(e));
                if (!sctx || !sctx->backend || !sctx->param_id) return;
                auto* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
                if (!target) return;

                const uint32_t now_tick = lv_tick_get();
                const bool pressed = lv_obj_has_state(target, LV_STATE_PRESSED);
                if (!sctx->has_sent || !pressed || (now_tick - sctx->last_send_tick_ms >= 35U)) {
                    commitSliderValue(*sctx, target);
                    return;
                }

                sctx->pending_release_commit = true;
                if (sctx->val_label) {
                    if (const ParameterDescriptor* d = ParameterRegistry::findById(sctx->param_id)) {
                        const float norm = static_cast<float>(lv_slider_get_value(target)) / 100.0f;
                        const ParameterValue local_pv = makeSliderParamValue(*d, sliderValue(*d, norm));
                        char buf[28];
                        ParameterRegistry::formatValue(*d, local_pv, buf, sizeof(buf));
                        lv_label_set_text(sctx->val_label, buf);
                    }
                }
            },
            LV_EVENT_VALUE_CHANGED);

        if (ctx) {
            lv_obj_add_event_cb(
                slider,
                [](lv_event_t* e) {
                    if (lv_event_get_code(e) != LV_EVENT_RELEASED) return;
                    auto* sctx = static_cast<SliderCtx*>(lv_event_get_user_data(e));
                    if (!sctx || !sctx->backend || !sctx->param_id || !sctx->pending_release_commit) return;
                    if (auto* target = static_cast<lv_obj_t*>(lv_event_get_target(e))) {
                        commitSliderValue(*sctx, target);
                    }
                },
                LV_EVENT_RELEASED, ctx);

            lv_obj_add_event_cb(
                slider,
                [](lv_event_t* e) {
                    auto* sctx = static_cast<SliderCtx*>(lv_event_get_user_data(e));
                    auto* obj = static_cast<lv_obj_t*>(lv_event_get_target(e));
                    if (!sctx || !sctx->backend || !sctx->param_id || !obj) return;
                    const auto* d = ParameterRegistry::findById(sctx->param_id);
                    if (!d) return;
                    ParameterValue value{};
                    sctx->backend->model().getValue(sctx->param_id, value);
                    char text[28];
                    ParameterRegistry::formatValue(*d, value, text, sizeof(text));
                    const FeatureStatus st = sctx->backend->parameterStatus(sctx->param_id);
                    if (!sctx->backend->isDemoMode() && st != FeatureStatus::Available) {
                        std::snprintf(text, sizeof(text), "%s", st == FeatureStatus::Unavailable ? "N/A" : "PLAN");
                    }
                    if (sctx->val_label) lv_label_set_text(sctx->val_label, text);
                    if (sctx->badge_box) {
                        lv_obj_set_style_bg_color(sctx->badge_box, lv_color_hex(featureStatusColor(st)), 0);
                        if (lv_obj_t* blbl = lv_obj_get_child(sctx->badge_box, 0)) {
                            lv_label_set_text_static(blbl, featureStatusLabel(st));
                            lv_obj_set_style_text_color(
                                blbl, lv_color_hex(st == FeatureStatus::Unavailable ? palette::kTextPrimary : palette::kBackground), 0);
                        }
                    }
                    if (!lv_obj_has_state(obj, LV_STATE_PRESSED)) {
                        lv_slider_set_value(obj, sliderPercent(*d, value.asFloat()), LV_ANIM_OFF);
                    }
                    if (sctx->backend->isParameterEditable(sctx->param_id)) lv_obj_remove_state(obj, LV_STATE_DISABLED);
                    else lv_obj_add_state(obj, LV_STATE_DISABLED);
                },
                LV_EVENT_REFRESH, ctx);
        }
    }
    return card;
}

} // namespace smk::p4::ui
