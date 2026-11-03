#include "ui_widgets.h"
#include <cmath>
#include <cstdio>

namespace smk::p4::ui {

namespace {
struct KnobCtx {
    IUiBackend* backend   = nullptr;
    const char* param_id  = nullptr;
    lv_obj_t*   val_label = nullptr;
};
} // namespace

lv_obj_t* createParameterKnob(lv_obj_t* parent, IUiBackend& backend, const char* param_id,
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

    createLabel(card, title, 6, 2, w - 16, &lv_font_montserrat_14,
                enabled ? palette::kTextPrimary : palette::kTextSecondary);

    lv_obj_t* arc = lv_arc_create(card);
    if (arc) {
        const int32_t arc_sz = (h >= 110) ? 56 : (h >= 94 ? 48 : 42);
        const int32_t arc_y = (h >= 94) ? 22 : 20;
        lv_obj_set_pos(arc, (w - arc_sz) / 2 - 4, arc_y);
        lv_obj_set_size(arc, arc_sz, arc_sz);
        lv_obj_set_ext_click_area(arc, 8);
        lv_arc_set_rotation(arc, 135);
        lv_arc_set_bg_angles(arc, 0, 270);
        lv_arc_set_range(arc, 0, 100);

        const float span = (desc->maximum > desc->minimum) ? (desc->maximum - desc->minimum) : 1.0f;
        const float cur = backend.model().getFloat(param_id, desc->default_value);
        const int32_t pct = static_cast<int32_t>(std::lround(((cur - desc->minimum) / span) * 100.0f));
        lv_arc_set_value(arc, pct);

        lv_obj_set_style_arc_color(arc, lv_color_hex(palette::kBorder), LV_PART_MAIN);
        lv_obj_set_style_arc_color(arc, lv_color_hex(palette::kAccentPrimary), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(arc, lv_color_hex(palette::kTextPrimary), LV_PART_KNOB);

        if (!enabled) {
            lv_obj_add_state(arc, LV_STATE_DISABLED);
        }

        ParameterValue pv{};
        backend.model().getValue(param_id, pv);
        char vbuf[24];
        ParameterRegistry::formatValue(*desc, pv, vbuf, sizeof(vbuf));
    if (!backend.isDemoMode() && status != FeatureStatus::Available) std::snprintf(vbuf,sizeof(vbuf),"PLAN");

        const int32_t val_w = (level == PresentationLevel::StageMinimal) ? (w - 16) : (w - 62);
        lv_obj_t* vlbl = createLabel(card, vbuf, 6, h - 26, val_w, &lv_font_montserrat_14, palette::kAccentPrimary);
        if (level == PresentationLevel::BlueprintVerbose) {
            createStatusBadge(card, status, w - 60, h - 26);
        }

        attachLvContext<KnobCtx>(
            arc, KnobCtx{&backend, desc->id, vlbl},
            [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
                auto* ctx = static_cast<KnobCtx*>(lv_event_get_user_data(e));
                if (!ctx || !ctx->backend || !ctx->param_id) return;
                const ParameterDescriptor* d = ParameterRegistry::findById(ctx->param_id);
                if (!d) return;
                auto* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
                const int32_t pct_val = lv_arc_get_value(target);
                const float span_v = (d->maximum > d->minimum) ? (d->maximum - d->minimum) : 1.0f;
                const float norm = static_cast<float>(pct_val) / 100.0f;
                const float mapped = d->minimum + norm * span_v;
                const UiCommandResult res = ctx->backend->requestParameterFloat(ctx->param_id, mapped);
                if (!res.accepted()) {
                    const float confirmed = ctx->backend->model().getFloat(ctx->param_id, d->default_value);
                    const int32_t conf_pct = static_cast<int32_t>(std::lround(((confirmed - d->minimum) / span_v) * 100.0f));
                    lv_arc_set_value(target, conf_pct);
                }
                if (ctx->val_label) {
                    ParameterValue cur_pv{};
                    ctx->backend->model().getValue(ctx->param_id, cur_pv);
                    char buf[24];
                    ParameterRegistry::formatValue(*d, cur_pv, buf, sizeof(buf));
                    lv_label_set_text(ctx->val_label, buf);
                }
            },
            LV_EVENT_VALUE_CHANGED);
    }

    return card;
}

} // namespace smk::p4::ui
