#include "ui_widgets.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace smk::p4::ui {

namespace {

enum class SliderUiState : uint8_t {
    Confirmed = 0,
    Dragging  = 1,
    Pending   = 2,
    Rejected  = 3
};

constexpr uint32_t kCoalesceWindowMs = 35U;
constexpr uint32_t kFallbackPendingTimeoutMs = 2500U;

struct DisabledSliderStyles {
    bool initialized = false;
    lv_style_t indicator;
    lv_style_t knob;
};

DisabledSliderStyles& disabledStyles() {
    static DisabledSliderStyles s{};
    if (!s.initialized) {
        s.initialized = true;
        lv_style_init(&s.indicator);
        lv_style_set_bg_color(&s.indicator, lv_color_hex(palette::kBorder));

        lv_style_init(&s.knob);
        lv_style_set_bg_color(&s.knob, lv_color_hex(palette::kTextSecondary));
    }
    return s;
}

float sliderValue(const ParameterDescriptor& d, int32_t pct) {
    const int32_t clamped_pct = std::clamp<int32_t>(pct, 0, 100);
    if (clamped_pct <= 0) return d.minimum;
    if (clamped_pct >= 100) return d.maximum;
    const float norm = static_cast<float>(clamped_pct) / 100.0f;
    if (d.scale == ParameterScale::Logarithmic && d.minimum > 0.0f && d.maximum > d.minimum) {
        const float v = d.minimum * std::pow(d.maximum / d.minimum, norm);
        return std::clamp(v, d.minimum, d.maximum);
    }
    float v = d.minimum + norm * (d.maximum - d.minimum);
    if (d.step > 0.0f && (d.maximum - d.minimum) >= d.step) {
        v = d.minimum + std::round((v - d.minimum) / d.step) * d.step;
    }
    return std::clamp(v, d.minimum, d.maximum);
}

int32_t sliderPercent(const ParameterDescriptor& d, float value) {
    const float finite_v = std::isfinite(value) ? value : d.default_value;
    const float v = std::clamp(finite_v, d.minimum, d.maximum);
    if (d.scale == ParameterScale::Logarithmic && d.minimum > 0.0f && d.maximum > d.minimum) {
        const float ratio = std::log(v / d.minimum) / std::log(d.maximum / d.minimum);
        return std::clamp<int32_t>(static_cast<int32_t>(std::lround(100.0f * ratio)), 0, 100);
    }
    if (d.maximum <= d.minimum) return 0;
    const float ratio = (v - d.minimum) / (d.maximum - d.minimum);
    return std::clamp<int32_t>(static_cast<int32_t>(std::lround(100.0f * ratio)), 0, 100);
}

bool valuesClose(const ParameterDescriptor& d, float a, float b) {
    const float span = std::max(1.0f, d.maximum - d.minimum);
    const float tol = std::max(1e-4f, (d.step > 0.0f) ? (d.step * 0.25f) : (span * 0.002f));
    return std::fabs(a - b) <= tol;
}

ParameterValue makeSliderParamValue(const ParameterDescriptor& d, float mapped) {
    if (d.type == ParameterValueType::Int) return ParameterValue::makeInt(static_cast<int32_t>(std::lround(mapped)));
    if (d.type == ParameterValueType::Enum) return ParameterValue::makeEnum(static_cast<int32_t>(std::lround(mapped)));
    if (d.type == ParameterValueType::Bool) return ParameterValue::makeBool(mapped >= 0.5f);
    return ParameterValue::makeFloat(mapped);
}

enum class ToneColor : uint8_t {
    Unset = 0,
    Primary,
    Secondary,
    Accent,
    Warning,
    Error
};

uint32_t toneToHex(ToneColor t) {
    switch (t) {
        case ToneColor::Primary:   return palette::kTextPrimary;
        case ToneColor::Secondary: return palette::kTextSecondary;
        case ToneColor::Accent:    return palette::kAccentPrimary;
        case ToneColor::Warning:   return palette::kWarning;
        case ToneColor::Error:     return palette::kErrorPanic;
        default:                   return palette::kTextPrimary;
    }
}

struct SliderCtx {
    IUiBackend*       backend               = nullptr;
    const char*       param_id              = nullptr;
    lv_obj_t*         title_label           = nullptr;
    lv_obj_t*         val_label             = nullptr;
    lv_obj_t*         state_label           = nullptr;
    lv_obj_t*         badge_box             = nullptr;
    uint32_t          last_send_tick_ms     = 0;
    uint32_t          pending_since_ms      = 0;
    uint32_t          pending_token         = 0;
    float             last_sent_value       = 0.0f;
    float             pending_value         = 0.0f;
    float             confirmed_before_send = 0.0f;
    float             last_confirmed_value  = 0.0f;
    PresentationLevel level                 = PresentationLevel::BlueprintVerbose;
    SliderUiState     ui_state              = SliderUiState::Confirmed;
    ToneColor         val_tone              = ToneColor::Unset;
    ToneColor         state_tone            = ToneColor::Unset;
    ToneColor         title_tone            = ToneColor::Unset;
    FeatureStatus     last_badge_status     = FeatureStatus::Available;
    bool              has_sent              = false;
    bool              pending_release_commit = false;
    bool              dragging              = false;
    bool              has_badge_status      = false;
};

void setDynamicLabel(lv_obj_t* lbl, const char* text, ToneColor tone, ToneColor& cached_tone) {
    if (!lbl || !text) return;
    if (std::strcmp(lv_label_get_text(lbl), text) != 0) {
        lv_label_set_text(lbl, text);
    }
    if (cached_tone != tone) {
        cached_tone = tone;
        lv_obj_set_style_text_color(lbl, lv_color_hex(toneToHex(tone)), 0);
    }
}

void setStaticStateLabel(lv_obj_t* lbl, const char* static_text, ToneColor tone, ToneColor& cached_tone) {
    if (!lbl || !static_text) return;
    if (std::strcmp(lv_label_get_text(lbl), static_text) != 0) {
        lv_label_set_text_static(lbl, static_text);
    }
    if (static_text[0] != '\0' && cached_tone != tone) {
        cached_tone = tone;
        lv_obj_set_style_text_color(lbl, lv_color_hex(toneToHex(tone)), 0);
    }
}

const char* lockedTagForParam(const IUiBackend& backend, const char* param_id) {
    if (std::strcmp(param_id, "fx.chorus.depth") == 0 &&
        backend.model().getInt("fx.chorus.mode", 0) == 0) {
        return "OFF";
    }
    if (std::strcmp(param_id, "fx.delay.time_ms") == 0 &&
        backend.model().getInt("fx.delay.mode", 0) == 1) {
        return "SYNC";
    }
    if (backend.parameterStatus(param_id) == FeatureStatus::Planned) {
        return "PLAN";
    }
    return "LOCK";
}

void formatConfirmedValue(const IUiBackend& backend, const ParameterDescriptor& d,
                          FeatureStatus status, char* out, size_t cap) {
    ParameterValue pv{};
    backend.model().getValue(d.id, pv);
    ParameterRegistry::formatValue(d, pv, out, cap);
    if (!backend.isDemoMode() && status != FeatureStatus::Available) {
        std::snprintf(out, cap, "%s", status == FeatureStatus::Unavailable ? "N/A" : "PLAN");
    }
}

void renderVisuals(SliderCtx& ctx, const ParameterDescriptor& d, float display_val, bool use_display_val) {
    const FeatureStatus status = ctx.backend->parameterStatus(ctx.param_id);
    const bool editable = ctx.backend->isParameterEditable(ctx.param_id);

    if (ctx.title_label) {
        const ToneColor ttone = editable ? ToneColor::Primary : ToneColor::Secondary;
        if (ctx.title_tone != ttone) {
            ctx.title_tone = ttone;
            lv_obj_set_style_text_color(ctx.title_label, lv_color_hex(toneToHex(ttone)), 0);
        }
    }

    char vbuf[28];
    if (use_display_val) {
        const ParameterValue pv = makeSliderParamValue(d, display_val);
        ParameterRegistry::formatValue(d, pv, vbuf, sizeof(vbuf));
    } else {
        formatConfirmedValue(*ctx.backend, d, status, vbuf, sizeof(vbuf));
    }

    const char* st_tag = "";
    ToneColor vtone = editable ? ToneColor::Accent : ToneColor::Secondary;
    ToneColor stone = ToneColor::Secondary;

    if (!editable) {
        st_tag = lockedTagForParam(*ctx.backend, ctx.param_id);
        vtone = ToneColor::Secondary;
        stone = ToneColor::Secondary;
    } else if (ctx.ui_state == SliderUiState::Dragging) {
        st_tag = "EDIT";
        vtone = ToneColor::Primary;
        stone = ToneColor::Primary;
    } else if (ctx.ui_state == SliderUiState::Pending) {
        st_tag = "WAIT";
        vtone = ToneColor::Warning;
        stone = ToneColor::Warning;
    } else if (ctx.ui_state == SliderUiState::Rejected) {
        st_tag = "REJ";
        vtone = ToneColor::Error;
        stone = ToneColor::Error;
    }

    if (ctx.val_label) {
        if (ctx.level == PresentationLevel::StageMinimal && !ctx.state_label) {
            char m_buf[32];
            if (ctx.ui_state == SliderUiState::Dragging) {
                std::snprintf(m_buf, sizeof(m_buf), "~%s", vbuf);
            } else if (ctx.ui_state == SliderUiState::Pending) {
                std::snprintf(m_buf, sizeof(m_buf), "*%s", vbuf);
            } else {
                std::snprintf(m_buf, sizeof(m_buf), "%s", vbuf);
            }
            setDynamicLabel(ctx.val_label, m_buf, vtone, ctx.val_tone);
        } else {
            setDynamicLabel(ctx.val_label, vbuf, vtone, ctx.val_tone);
        }
    }

    if (ctx.state_label) {
        setStaticStateLabel(ctx.state_label, st_tag, stone, ctx.state_tone);
    }

    if (ctx.badge_box && (!ctx.has_badge_status || ctx.last_badge_status != status)) {
        ctx.has_badge_status = true;
        ctx.last_badge_status = status;
        lv_obj_set_style_bg_color(ctx.badge_box, lv_color_hex(featureStatusColor(status)), 0);
        if (lv_obj_t* blbl = lv_obj_get_child(ctx.badge_box, 0)) {
            lv_label_set_text_static(blbl, featureStatusLabel(status));
            lv_obj_set_style_text_color(
                blbl, lv_color_hex(status == FeatureStatus::Unavailable ? palette::kTextPrimary : palette::kBackground), 0);
        }
    }
}

void syncThumbAndConfirmedVisuals(SliderCtx& ctx, lv_obj_t* target, const ParameterDescriptor& d) {
    const float confirmed = ctx.backend->model().getFloat(ctx.param_id, d.default_value);
    ctx.last_confirmed_value = confirmed;
    const int32_t target_pct = sliderPercent(d, confirmed);
    if (lv_slider_get_value(target) != target_pct) {
        lv_slider_set_value(target, target_pct, LV_ANIM_OFF);
    }
    renderVisuals(ctx, d, confirmed, false);
}

void commitSliderValue(SliderCtx& ctx, lv_obj_t* target, const ParameterDescriptor& d, float mapped) {
    const float confirmed_before = ctx.backend->model().getFloat(ctx.param_id, d.default_value);
    ctx.last_confirmed_value = confirmed_before;
    const UiCommandResult res = ctx.backend->requestParameterFloat(ctx.param_id, mapped);
    const uint32_t now_tick = lv_tick_get();
    ctx.last_send_tick_ms = now_tick;
    ctx.pending_release_commit = false;

    if (!res.accepted()) {
        ctx.has_sent = false;
        ctx.pending_token = 0;
        ctx.ui_state = (res.state == UiCommandState::Rejected) ? SliderUiState::Rejected : SliderUiState::Confirmed;
        syncThumbAndConfirmedVisuals(ctx, target, d);
        return;
    }

    ctx.has_sent = true;
    ctx.last_sent_value = mapped;

    if (res.state == UiCommandState::Requested) {
        ctx.pending_value = mapped;
        ctx.pending_token = res.token;
        ctx.pending_since_ms = now_tick;
        ctx.confirmed_before_send = confirmed_before;
        ctx.ui_state = SliderUiState::Pending;
        renderVisuals(ctx, d, mapped, true);
        return;
    }

    ctx.pending_token = 0;
    ctx.ui_state = SliderUiState::Confirmed;
    syncThumbAndConfirmedVisuals(ctx, target, d);
}

void resolvePendingOnRefresh(SliderCtx& ctx, const ParameterDescriptor& d, float confirmed) {
    const char* last_id = ctx.backend->lastCommandParameterId();
    const UiCommandState last_st = ctx.backend->lastCommandState();
    const uint32_t last_tok = ctx.backend->lastCommandToken();
    const bool id_matches = last_id && (std::strcmp(last_id, ctx.param_id) == 0);

    if (ctx.ui_state == SliderUiState::Pending) {
        if (!ctx.backend->isConnected()) {
            ctx.ui_state = SliderUiState::Rejected;
            ctx.has_sent = false;
            ctx.pending_token = 0;
        } else if (id_matches) {
            if (last_st == UiCommandState::Confirmed &&
                (ctx.pending_token == 0 || last_tok == 0 || last_tok >= ctx.pending_token)) {
                ctx.ui_state = SliderUiState::Confirmed;
                ctx.pending_token = 0;
            } else if (last_st == UiCommandState::Rejected || last_st == UiCommandState::Unsupported) {
                ctx.ui_state = (last_st == UiCommandState::Rejected) ? SliderUiState::Rejected : SliderUiState::Confirmed;
                ctx.has_sent = false;
                ctx.pending_token = 0;
            } else if (static_cast<uint32_t>(lv_tick_get() - ctx.pending_since_ms) >= kFallbackPendingTimeoutMs) {
                ctx.ui_state = SliderUiState::Rejected;
                ctx.has_sent = false;
                ctx.pending_token = 0;
            }
        } else {
            if (valuesClose(d, confirmed, ctx.pending_value) ||
                !valuesClose(d, confirmed, ctx.confirmed_before_send)) {
                ctx.ui_state = SliderUiState::Confirmed;
                ctx.pending_token = 0;
            } else if (static_cast<uint32_t>(lv_tick_get() - ctx.pending_since_ms) >= kFallbackPendingTimeoutMs) {
                ctx.ui_state = SliderUiState::Rejected;
                ctx.has_sent = false;
                ctx.pending_token = 0;
            }
        }
    } else if (ctx.ui_state == SliderUiState::Confirmed) {
        if (id_matches && last_st == UiCommandState::Requested) {
            ctx.ui_state = SliderUiState::Pending;
            ctx.pending_value = confirmed;
            ctx.pending_token = last_tok;
            ctx.pending_since_ms = lv_tick_get();
            ctx.confirmed_before_send = confirmed;
        }
    } else if (ctx.ui_state == SliderUiState::Rejected) {
        if (!valuesClose(d, confirmed, ctx.last_confirmed_value)) {
            ctx.ui_state = SliderUiState::Confirmed;
        }
    }
}

void onSliderEvent(lv_event_t* e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_VALUE_CHANGED && code != LV_EVENT_RELEASED &&
        code != LV_EVENT_PRESS_LOST && code != LV_EVENT_CANCEL && code != LV_EVENT_INDEV_RESET &&
        code != LV_EVENT_REFRESH) {
        return;
    }

    auto* ctx = static_cast<SliderCtx*>(lv_event_get_user_data(e));
    auto* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    if (!ctx || !ctx->backend || !ctx->param_id || !target) return;

    const ParameterDescriptor* d = ParameterRegistry::findById(ctx->param_id);
    if (!d) return;

    if (code == LV_EVENT_PRESSED) {
        if (!ctx->backend->isParameterEditable(ctx->param_id) ||
            lv_obj_has_state(target, LV_STATE_DISABLED)) {
            ctx->dragging = false;
            ctx->pending_release_commit = false;
            return;
        }
        ctx->dragging = true;
        ctx->has_sent = false;
        ctx->pending_release_commit = false;
        return;
    }

    if (code == LV_EVENT_VALUE_CHANGED) {
        if (!ctx->backend->isParameterEditable(ctx->param_id) ||
            lv_obj_has_state(target, LV_STATE_DISABLED)) {
            ctx->dragging = false;
            ctx->pending_release_commit = false;
            ctx->has_sent = false;
            ctx->ui_state = SliderUiState::Confirmed;
            syncThumbAndConfirmedVisuals(*ctx, target, *d);
            return;
        }

        const float mapped = sliderValue(*d, lv_slider_get_value(target));
        const bool lv_pressed = lv_obj_has_state(target, LV_STATE_PRESSED);
        const bool active_drag = ctx->dragging || lv_pressed || lv_slider_is_dragged(target);
        if (lv_pressed) {
            ctx->dragging = true;
        }

        if (ctx->has_sent && valuesClose(*d, mapped, ctx->last_sent_value)) {
            ctx->pending_release_commit = false;
            if (ctx->ui_state == SliderUiState::Dragging) {
                const bool still_pending =
                    ctx->backend->lastCommandParameterId() &&
                    std::strcmp(ctx->backend->lastCommandParameterId(), ctx->param_id) == 0 &&
                    ctx->backend->lastCommandState() == UiCommandState::Requested;
                ctx->ui_state = still_pending ? SliderUiState::Pending : SliderUiState::Confirmed;
            }
            if (ctx->ui_state == SliderUiState::Pending) {
                renderVisuals(*ctx, *d, ctx->pending_value, true);
            } else {
                renderVisuals(*ctx, *d, ctx->backend->model().getFloat(ctx->param_id, d->default_value), false);
            }
            return;
        }

        // Defer inside PRESS_LOST/RELEASED teardown (ctx->dragging && !lv_pressed) so PRESS_LOST never commits.
        const bool in_teardown_move = ctx->dragging && !lv_pressed;
        const uint32_t now_tick = lv_tick_get();
        if (!in_teardown_move &&
            (!ctx->has_sent || !active_drag || static_cast<uint32_t>(now_tick - ctx->last_send_tick_ms) >= kCoalesceWindowMs)) {
            commitSliderValue(*ctx, target, *d, mapped);
            return;
        }

        ctx->pending_release_commit = true;
        ctx->ui_state = SliderUiState::Dragging;
        renderVisuals(*ctx, *d, mapped, true);
        return;
    }

    if (code == LV_EVENT_RELEASED) {
        ctx->dragging = false;
        if (!ctx->backend->isParameterEditable(ctx->param_id) ||
            lv_obj_has_state(target, LV_STATE_DISABLED)) {
            ctx->pending_release_commit = false;
            ctx->has_sent = false;
            ctx->ui_state = SliderUiState::Confirmed;
            syncThumbAndConfirmedVisuals(*ctx, target, *d);
            return;
        }
        if (ctx->pending_release_commit) {
            const float mapped = sliderValue(*d, lv_slider_get_value(target));
            if (!ctx->has_sent || !valuesClose(*d, mapped, ctx->last_sent_value)) {
                commitSliderValue(*ctx, target, *d, mapped);
                return;
            }
            ctx->pending_release_commit = false;
        }
        if (ctx->ui_state == SliderUiState::Dragging) {
            const bool still_pending =
                ctx->backend->lastCommandParameterId() &&
                std::strcmp(ctx->backend->lastCommandParameterId(), ctx->param_id) == 0 &&
                ctx->backend->lastCommandState() == UiCommandState::Requested;
            ctx->ui_state = still_pending ? SliderUiState::Pending : SliderUiState::Confirmed;
        }
        if (ctx->ui_state == SliderUiState::Pending) {
            renderVisuals(*ctx, *d, ctx->pending_value, true);
        } else {
            syncThumbAndConfirmedVisuals(*ctx, target, *d);
        }
        return;
    }

    if (code == LV_EVENT_PRESS_LOST || code == LV_EVENT_CANCEL || code == LV_EVENT_INDEV_RESET) {
        ctx->dragging = false;
        ctx->pending_release_commit = false;
        ctx->has_sent = false;
        if (code == LV_EVENT_CANCEL || ctx->ui_state != SliderUiState::Pending) {
            ctx->ui_state = SliderUiState::Confirmed;
        }
        if (ctx->ui_state == SliderUiState::Pending) {
            const int32_t pct = sliderPercent(*d, ctx->pending_value);
            if (lv_slider_get_value(target) != pct) lv_slider_set_value(target, pct, LV_ANIM_OFF);
            renderVisuals(*ctx, *d, ctx->pending_value, true);
        } else {
            syncThumbAndConfirmedVisuals(*ctx, target, *d);
        }
        return;
    }

    if (code == LV_EVENT_REFRESH) {
        const bool editable = ctx->backend->isParameterEditable(ctx->param_id);
        const bool was_disabled = lv_obj_has_state(target, LV_STATE_DISABLED);
        if (editable && was_disabled) lv_obj_remove_state(target, LV_STATE_DISABLED);
        else if (!editable && !was_disabled) lv_obj_add_state(target, LV_STATE_DISABLED);

        const float confirmed = ctx->backend->model().getFloat(ctx->param_id, d->default_value);
        const bool is_dragging = ctx->dragging || lv_obj_has_state(target, LV_STATE_PRESSED) || lv_slider_is_dragged(target);

        if (!editable) {
            ctx->dragging = false;
            ctx->pending_release_commit = false;
            ctx->has_sent = false;
            ctx->pending_token = 0;
            ctx->ui_state = SliderUiState::Confirmed;
            syncThumbAndConfirmedVisuals(*ctx, target, *d);
            return;
        }

        if (is_dragging) {
            ctx->last_confirmed_value = confirmed;
            if (ctx->title_label && ctx->title_tone != ToneColor::Primary) {
                ctx->title_tone = ToneColor::Primary;
                lv_obj_set_style_text_color(ctx->title_label, lv_color_hex(palette::kTextPrimary), 0);
            }
            return;
        }

        resolvePendingOnRefresh(*ctx, *d, confirmed);
        ctx->last_confirmed_value = confirmed;

        if (ctx->ui_state == SliderUiState::Pending) {
            renderVisuals(*ctx, *d, ctx->pending_value, true);
        } else {
            syncThumbAndConfirmedVisuals(*ctx, target, *d);
        }
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

    char vbuf[28];
    formatConfirmedValue(backend, *desc, status, vbuf, sizeof(vbuf));

    lv_obj_t* title_lbl = nullptr;
    lv_obj_t* vlbl = nullptr;
    lv_obj_t* slbl = nullptr;
    lv_obj_t* badge_box = nullptr;

    if (level == PresentationLevel::StageMinimal) {
        title_lbl = createLabel(card, title, 8, 4, w - 96, &lv_font_montserrat_14,
                                enabled ? palette::kTextPrimary : palette::kTextSecondary);
        const lv_font_t* val_font = (h >= 72) ? &lv_font_montserrat_20 : &lv_font_montserrat_14;
        vlbl = createLabel(card, vbuf, w - 88, (h >= 72) ? 2 : 4, 76, val_font,
                           enabled ? palette::kAccentPrimary : palette::kTextSecondary);
        if (vlbl) lv_obj_set_style_text_align(vlbl, LV_TEXT_ALIGN_RIGHT, 0);
    } else if (level == PresentationLevel::StageStacked) {
        const bool large = (h >= 90);
        title_lbl = createLabel(card, title, 6, 4, w - 16, &lv_font_montserrat_14,
                                enabled ? palette::kTextPrimary : palette::kTextSecondary);
        const lv_font_t* val_font = large ? &lv_font_montserrat_20 : &lv_font_montserrat_14;
        vlbl = createLabel(card, vbuf, 6, large ? 25 : 24, w - 82, val_font,
                           enabled ? palette::kAccentPrimary : palette::kTextSecondary);
        slbl = lv_label_create(card);
        if (slbl) {
            lv_obj_set_pos(slbl, w - 72, large ? 29 : 24);
            lv_obj_set_size(slbl, 56, 20);
            lv_obj_set_style_text_font(slbl, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_align(slbl, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_long_mode(slbl, LV_LABEL_LONG_MODE_CLIP);
            lv_label_set_text_static(slbl, "");
        }
    } else {
        title_lbl = createLabel(card, title, 6, 4, w - 64, &lv_font_montserrat_14,
                                enabled ? palette::kTextPrimary : palette::kTextSecondary);
        badge_box = createStatusBadge(card, status, w - 62, 4);
        vlbl = createLabel(card, vbuf, 6, 24, w - 76, &lv_font_montserrat_14,
                           enabled ? palette::kAccentPrimary : palette::kTextSecondary);
        slbl = lv_label_create(card);
        if (slbl) {
            lv_obj_set_pos(slbl, w - 68, 24);
            lv_obj_set_size(slbl, 54, 20);
            lv_obj_set_style_text_font(slbl, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_align(slbl, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_long_mode(slbl, LV_LABEL_LONG_MODE_CLIP);
            lv_label_set_text_static(slbl, "");
        }
    }

    lv_obj_t* slider = lv_slider_create(card);
    if (slider) {
        auto& dst = disabledStyles();
        lv_obj_remove_style_all(slider);
        lv_obj_add_style(slider, &theme().slider_main, LV_PART_MAIN);
        lv_obj_add_style(slider, &theme().slider_indicator, LV_PART_INDICATOR);
        lv_obj_add_style(slider, &dst.indicator, static_cast<lv_style_selector_t>(LV_PART_INDICATOR) | LV_STATE_DISABLED);
        lv_obj_add_style(slider, &theme().slider_knob, LV_PART_KNOB);
        lv_obj_add_style(slider, &dst.knob, static_cast<lv_style_selector_t>(LV_PART_KNOB) | LV_STATE_DISABLED);

        const bool stage = (level != PresentationLevel::BlueprintVerbose) && (h >= 68);
        const int32_t bar_h = stage ? 18 : 14;
        const int32_t bar_y = stage ? (h - 32) : (h - 26);
        lv_obj_set_pos(slider, 10, bar_y);
        lv_obj_set_size(slider, w - 28, bar_h);
        lv_obj_set_ext_click_area(slider, 14);
        lv_slider_set_range(slider, 0, 100);

        const float cur = backend.model().getFloat(param_id, desc->default_value);
        lv_slider_set_value(slider, sliderPercent(*desc, cur), LV_ANIM_OFF);

        if (!enabled) {
            lv_obj_add_state(slider, LV_STATE_DISABLED);
        }

        SliderCtx init_ctx{};
        init_ctx.backend = &backend;
        init_ctx.param_id = desc->id;
        init_ctx.title_label = title_lbl;
        init_ctx.val_label = vlbl;
        init_ctx.state_label = slbl;
        init_ctx.badge_box = badge_box;
        init_ctx.level = level;
        init_ctx.last_confirmed_value = cur;
        init_ctx.title_tone = enabled ? ToneColor::Primary : ToneColor::Secondary;
        init_ctx.val_tone = enabled ? ToneColor::Accent : ToneColor::Secondary;
        init_ctx.last_badge_status = status;
        init_ctx.has_badge_status = (badge_box != nullptr);

        if (SliderCtx* ctx = attachLvContext<SliderCtx>(slider, init_ctx, onSliderEvent, LV_EVENT_ALL)) {
            if (backend.lastCommandParameterId() &&
                std::strcmp(backend.lastCommandParameterId(), desc->id) == 0 &&
                backend.lastCommandState() == UiCommandState::Requested) {
                ctx->ui_state = SliderUiState::Pending;
                ctx->pending_value = cur;
                ctx->pending_token = backend.lastCommandToken();
                ctx->pending_since_ms = lv_tick_get();
                ctx->confirmed_before_send = cur;
            }
            renderVisuals(*ctx, *desc, cur, ctx->ui_state == SliderUiState::Pending);
        }
    }
    return card;
}

} // namespace smk::p4::ui
