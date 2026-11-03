#include "ui_widgets.h"
#include <cmath>
#include <cstdio>

namespace smk::p4::ui {

namespace {
struct KnobCtx {
    UiDemoBackend* backend   = nullptr;
    const char*    param_id  = nullptr;
    lv_obj_t*      val_label = nullptr;
};
} // namespace

lv_obj_t* createParameterKnob(lv_obj_t* parent, UiDemoBackend& backend, const char* param_id,
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

    createLabel(card, desc->name, 4, 2, w - 12, &lv_font_montserrat_14,
                enabled ? palette::kTextPrimary : palette::kTextSecondary);

    lv_obj_t* arc = lv_arc_create(card);
    if (arc) {
        const int32_t arc_sz = (h >= 110) ? 54 : 46;
        lv_obj_set_pos(arc, (w - arc_sz) / 2 - 4, 22);
        lv_obj_set_size(arc, arc_sz, arc_sz);
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
        lv_obj_t* vlbl = createLabel(card, vbuf, 4, h - 26, w - 68, &lv_font_montserrat_14, palette::kAccentPrimary);
        createStatusBadge(card, status, w - 72, h - 26);

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
                const float norm = static_cast<float>(pct_val) / 100.0f;
                const float mapped = d->minimum + norm * (d->maximum - d->minimum);
                ctx->backend->model().setFloat(ctx->param_id, mapped);
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
