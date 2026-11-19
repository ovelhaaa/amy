#include "ui_screens.h"
#include "ui_widgets.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace smk::p4::ui {
namespace {

constexpr uint8_t kFxModuleCount = 4;
constexpr const char* kModuleLabels[kFxModuleCount] = {"DELAY", "REVERB", "CHORUS", "MASTER"};

constexpr const char* kDelayParams[] = {
    "fx.delay.mode", "fx.delay.division", "fx.delay.time_ms", "fx.delay.feedback", "fx.delay.mix"
};
constexpr const char* kReverbParams[] = {
    "fx.reverb.size", "fx.reverb.mix"
};
constexpr const char* kChorusParams[] = {
    "fx.chorus.mode", "fx.chorus.depth"
};
constexpr const char* kMasterParams[] = {
    "fx.bypass_all", "perf.master_volume", "fx.master_tone", "fx.eq.low_db", "fx.eq.mid_db", "fx.eq.high_db"
};

struct ModuleParamList {
    const char* const* ids;
    uint8_t count;
};

ModuleParamList moduleParams(uint8_t mod) {
    switch (mod) {
        case 0: return {kDelayParams, static_cast<uint8_t>(sizeof(kDelayParams) / sizeof(kDelayParams[0]))};
        case 1: return {kReverbParams, static_cast<uint8_t>(sizeof(kReverbParams) / sizeof(kReverbParams[0]))};
        case 2: return {kChorusParams, static_cast<uint8_t>(sizeof(kChorusParams) / sizeof(kChorusParams[0]))};
        case 3: return {kMasterParams, static_cast<uint8_t>(sizeof(kMasterParams) / sizeof(kMasterParams[0]))};
        default: return {kDelayParams, 5};
    }
}

const char* commandStateTextPt(UiCommandState state) {
    switch (state) {
        case UiCommandState::Confirmed:   return "CONFIRMADO";
        case UiCommandState::Requested:   return "AGUARDANDO";
        case UiCommandState::Rejected:    return "REJEITADO";
        case UiCommandState::Unsupported: return "INDISPONIVEL";
        default:                          return "DESCONHECIDO";
    }
}

void updateStaticBufLabel(lv_obj_t* label, char* buf, size_t cap, const char* text) {
    if (!label || !buf || cap == 0 || !text) return;
    if (std::strcmp(buf, text) == 0) return;
    std::snprintf(buf, cap, "%s", text);
    lv_label_set_text_static(label, buf);
    lv_obj_invalidate(label);
}

void setStaticButtonTextIfChanged(lv_obj_t* label, const char* static_text) {
    if (!label || !static_text) return;
    const char* cur = lv_label_get_text(label);
    if (!cur || std::strcmp(cur, static_text) != 0) {
        lv_label_set_text_static(label, static_text);
    }
}

void setTextColorIfChanged(lv_obj_t* obj, uint32_t& cached_hex, uint32_t next_hex) {
    if (!obj || cached_hex == next_hex) return;
    cached_hex = next_hex;
    lv_obj_set_style_text_color(obj, lv_color_hex(next_hex), 0);
}

void setDisabledIfChanged(lv_obj_t* obj, bool disabled) {
    if (!obj) return;
    const bool has = lv_obj_has_state(obj, LV_STATE_DISABLED);
    if (disabled && !has) lv_obj_add_state(obj, LV_STATE_DISABLED);
    else if (!disabled && has) lv_obj_remove_state(obj, LV_STATE_DISABLED);
}

void setHiddenIfChanged(lv_obj_t* obj, bool hidden) {
    if (!obj) return;
    const bool has = lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN);
    if (hidden && !has) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else if (!hidden && has) lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

class EffectsScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::Effects; }

    bool build(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav) override {
        backend_ = &backend;
        nav_ = &nav;
        active_module_ = static_cast<uint8_t>(nav.subTab(ScreenId::Effects) % kFxModuleCount);
        clearDetailPointers();

        // 1. Allocate long-lived sidebar container and permanent static children first (LIFO pool order).
        sidebar_ = lv_obj_create(parent);
        if (!sidebar_) return false;
        applyPanelStyle(sidebar_, false);
        lv_obj_set_pos(sidebar_, 4, 4);
        lv_obj_set_size(sidebar_, 152, 380);
        lv_obj_set_style_pad_all(sidebar_, 0, 0);

        detail_panel_ = lv_obj_create(parent);
        if (!detail_panel_) return false;
        applyScreenStyle(detail_panel_);
        lv_obj_set_pos(detail_panel_, 160, 4);
        lv_obj_set_size(detail_panel_, 636, 380);
        lv_obj_set_style_pad_all(detail_panel_, 0, 0);

        createStaticLabel(sidebar_, "EFEITOS", 8, 6, 136, palette::kTextSecondary);

        for (uint8_t i = 0; i < kFxModuleCount; ++i) {
            sidebar_btns_[i] = createSidebarButton(
                sidebar_, kModuleLabels[i], 6, 26 + static_cast<int32_t>(i) * 56, 140, 50,
                i == active_module_);
            if (sidebar_btns_[i]) {
                attachLvContext<ActionCtx>(sidebar_btns_[i], ActionCtx{this, ActionKind::SelectModule, int32_t(i)},
                                           onActionClicked, LV_EVENT_CLICKED);
            }
        }

        createStaticLabel(sidebar_, "CADEIA FIXA:", 6, 254, 140, palette::kTextSecondary);
        createStaticLabel(sidebar_, "CH>DLY>REV>EQ", 6, 274, 140, palette::kTextPrimary);
        chain_status_lbl_ = createStaticLabel(sidebar_, "ATIVA", 6, 296, 140, palette::kActive);
        chain_bypassed_known_ = false;

        lv_obj_t* ret_btn = lv_button_create(sidebar_);
        if (ret_btn) {
            lv_obj_remove_style_all(ret_btn);
            lv_obj_add_style(ret_btn, &theme().button, 0);
            lv_obj_add_style(ret_btn, &theme().button_accent, LV_STATE_CHECKED);
            lv_obj_set_pos(ret_btn, 6, 334);
            lv_obj_set_size(ret_btn, 140, 40);
            lv_obj_remove_flag(ret_btn, LV_OBJ_FLAG_SCROLLABLE);
            if (lv_obj_t* lbl = lv_label_create(ret_btn)) {
                lv_label_set_text_static(lbl, "< TOCAR");
                lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
                lv_obj_center(lbl);
            }
            attachLvContext<ActionCtx>(ret_btn, ActionCtx{this, ActionKind::ReturnTocar, 0},
                                       onActionClicked, LV_EVENT_CLICKED);
        }

        // 2. Build only the active FX module panel inside detail_panel_.
        buildModulePanel();
        return true;
    }

    void refresh(IUiBackend& backend) override {
        backend_ = &backend;
        const bool bypass_all = backend.model().getBool("fx.bypass_all", false);
        IUiBackend::DelayStatus status{};
        const bool has_ds = backend.delayStatus(status);
        const bool effective_bypass = bypass_all || (has_ds && status.bypassed);

        if (chain_status_lbl_ && (!chain_bypassed_known_ || chain_bypassed_cached_ != effective_bypass)) {
            chain_bypassed_known_ = true;
            chain_bypassed_cached_ = effective_bypass;
            lv_label_set_text_static(chain_status_lbl_, effective_bypass ? "BYPASS" : "ATIVA");
            lv_obj_set_style_text_color(
                chain_status_lbl_, lv_color_hex(effective_bypass ? palette::kWarning : palette::kActive), 0);
        }

        const int bpm = backend.model().getInt("perf.bpm", 120);
        uint32_t notice_color = palette::kTextSecondary;

        if (active_module_ == 0) {
            if (timing_) {
                if (has_ds) {
                    char timing[96];
                    if (status.sync) {
                        if (std::fabs(status.applied_ms - status.target_ms) > 0.5f) {
                            std::snprintf(timing, sizeof(timing), "%d BPM | Sync %.0f ms (DSP %.0f ms)",
                                          bpm, double(status.target_ms), double(status.applied_ms));
                        } else {
                            std::snprintf(timing, sizeof(timing), "%d BPM | Sync: %.0f ms",
                                          bpm, double(status.applied_ms));
                        }
                    } else {
                        std::snprintf(timing, sizeof(timing), "%d BPM | Delay: %.0f ms",
                                      bpm, double(status.applied_ms));
                    }
                    updateStaticBufLabel(timing_, timing_buf_, sizeof(timing_buf_), timing);
                } else {
                    updateStaticBufLabel(timing_, timing_buf_, sizeof(timing_buf_),
                                         backend.isDemoMode() ? "Modo demonstracao" : "Aguardando motor de audio");
                }
            }
            if (notice_) {
                const char* msg = "Tempo livre ativo | feedback e mix livres";
                if (effective_bypass) {
                    msg = "Bypass global ativo | efeitos em espera";
                    notice_color = palette::kWarning;
                } else if (has_ds && status.out_of_range) {
                    msg = "Tempo fora da faixa | mantendo limite seguro";
                    notice_color = palette::kWarning;
                } else if ((has_ds && status.sync) || backend.model().getInt("fx.delay.mode", 0) == 1) {
                    msg = "Sync ativo | tempo manual bloqueado";
                    notice_color = palette::kWarning;
                }
                setTextColorIfChanged(notice_, notice_color_cached_, notice_color);
                updateStaticBufLabel(notice_, notice_buf_, sizeof(notice_buf_), msg);
            }
        } else if (active_module_ == 1) {
            updateStaticBufLabel(timing_, timing_buf_, sizeof(timing_buf_),
                                 effective_bypass ? "Bypass global ativo" : "Cadeia fixa: CH>DLY>REV>EQ");
            const char* msg = effective_bypass
                                  ? "FX em bypass global (Master)"
                                  : "Reverb sem bypass individual | ajuste via Size e Mix";
            notice_color = effective_bypass ? palette::kWarning : palette::kTextSecondary;
            setTextColorIfChanged(notice_, notice_color_cached_, notice_color);
            updateStaticBufLabel(notice_, notice_buf_, sizeof(notice_buf_), msg);
        } else if (active_module_ == 2) {
            const int32_t mode = backend.model().getInt("fx.chorus.mode", 0);
            updateStaticBufLabel(timing_, timing_buf_, sizeof(timing_buf_),
                                 effective_bypass ? "Bypass global ativo"
                                                  : (mode == 0 ? "Chorus desligado" : "Chorus ativo na cadeia"));
            const char* msg = "Chorus ativo | Depth disponivel";
            if (effective_bypass) {
                msg = "FX em bypass global (Master)";
                notice_color = palette::kWarning;
            } else if (mode == 0) {
                msg = "Chorus desligado | Depth bloqueado (valor preservado)";
                notice_color = palette::kWarning;
            } else {
                notice_color = palette::kActive;
            }
            setTextColorIfChanged(notice_, notice_color_cached_, notice_color);
            updateStaticBufLabel(notice_, notice_buf_, sizeof(notice_buf_), msg);

            setTextColorIfChanged(chorus_note_title_, chorus_title_color_cached_,
                                  (mode == 0) ? palette::kWarning : palette::kActive);
            updateStaticBufLabel(chorus_note_title_, chorus_note_title_buf_, sizeof(chorus_note_title_buf_),
                                 (mode == 0) ? "CHORUS DESLIGADO" : "CHORUS ATIVO");
            updateStaticBufLabel(chorus_note_body_, chorus_note_body_buf_, sizeof(chorus_note_body_buf_),
                                 (mode == 0)
                                     ? "Chorus desligado no motor. Depth fica bloqueado e preserva o valor salvo."
                                     : "Chorus ativo no motor. Modos: Classic, Juno I/II, Ensemble e Vibrato.");
        } else if (active_module_ == 3) {
            updateStaticBufLabel(timing_, timing_buf_, sizeof(timing_buf_),
                                 effective_bypass ? "Bypass global ativo" : "Cadeia fixa: CH>DLY>REV>EQ");
            notice_color = effective_bypass ? palette::kWarning : palette::kTextSecondary;
            setTextColorIfChanged(notice_, notice_color_cached_, notice_color);
            updateStaticBufLabel(notice_, notice_buf_, sizeof(notice_buf_),
                                 effective_bypass
                                     ? "Bypass global ativo | todos os FX silenciados"
                                     : "Volume, Tilt Tone, EQ de 3 bandas e Bypass global");
        }

        refreshInspector();
    }

    void onDestroy() override {
        clearDetailPointers();
        for (auto& b : sidebar_btns_) b = nullptr;
        chain_status_lbl_ = nullptr;
        sidebar_ = nullptr;
        detail_panel_ = nullptr;
        backend_ = nullptr;
        nav_ = nullptr;
    }

private:
    enum class ActionKind : uint8_t {
        SelectModule,
        ReturnTocar,
        ToggleInspector,
        CycleFocus,
        FineAdjust
    };

    struct ActionCtx {
        EffectsScreen* self = nullptr;
        ActionKind kind = ActionKind::SelectModule;
        int32_t arg = 0;
    };

    static lv_obj_t* createStaticLabel(lv_obj_t* parent, const char* static_text,
                                       int32_t x, int32_t y, int32_t w, uint32_t color_hex,
                                       const lv_font_t* font = &lv_font_montserrat_14) {
        lv_obj_t* lbl = lv_label_create(parent);
        if (!lbl) return nullptr;
        lv_obj_set_pos(lbl, x, y);
        lv_obj_set_size(lbl, w, lv_font_get_line_height(font) + 4);
        lv_obj_set_style_text_font(lbl, font, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(color_hex), 0);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_MODE_CLIP);
        lv_label_set_text_static(lbl, static_text ? static_text : "");
        return lbl;
    }

    static lv_obj_t* createSidebarButton(lv_obj_t* parent, const char* static_label,
                                         int32_t x, int32_t y, int32_t w, int32_t h, bool checked,
                                         lv_obj_t** out_label = nullptr) {
        lv_obj_t* btn = lv_button_create(parent);
        if (!btn) return nullptr;
        lv_obj_remove_style_all(btn);
        lv_obj_add_style(btn, &theme().button, 0);
        lv_obj_add_style(btn, &theme().button_active, LV_STATE_CHECKED);
        lv_obj_set_style_opa(btn, LV_OPA_50, LV_STATE_DISABLED);
        lv_obj_set_pos(btn, x, y);
        lv_obj_set_size(btn, w, h);
        lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        if (checked) lv_obj_add_state(btn, LV_STATE_CHECKED);

        lv_obj_t* lbl = lv_label_create(btn);
        if (lbl) {
            lv_label_set_text_static(lbl, static_label ? static_label : "");
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
            lv_obj_center(lbl);
        }
        if (out_label) *out_label = lbl;
        return btn;
    }

    static lv_obj_t* createInfoBox(lv_obj_t* parent, const char* title, const char* body,
                                   int32_t x, int32_t y, int32_t w, int32_t h,
                                   uint32_t title_color_hex,
                                   lv_obj_t** out_title = nullptr,
                                   lv_obj_t** out_body = nullptr) {
        lv_obj_t* card = lv_obj_create(parent);
        if (!card) return nullptr;
        applyPanelStyle(card, false);
        lv_obj_set_pos(card, x, y);
        lv_obj_set_size(card, w, h);

        lv_obj_t* t = createStaticLabel(card, title, 2, 0, w - 16, title_color_hex);
        lv_obj_t* b = lv_label_create(card);
        if (b) {
            lv_label_set_long_mode(b, LV_LABEL_LONG_MODE_WRAP);
            lv_obj_set_pos(b, 2, 20);
            lv_obj_set_size(b, w - 16, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(b, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(b, lv_color_hex(palette::kTextSecondary), 0);
            lv_label_set_text_static(b, body ? body : "");
        }
        if (out_title) *out_title = t;
        if (out_body) *out_body = b;
        return card;
    }

    static void onActionClicked(lv_event_t* e) {
        if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
        auto* ctx = static_cast<ActionCtx*>(lv_event_get_user_data(e));
        if (!ctx || !ctx->self) return;
        EffectsScreen& self = *ctx->self;
        switch (ctx->kind) {
            case ActionKind::SelectModule:
                self.selectModule(static_cast<uint8_t>(ctx->arg));
                break;
            case ActionKind::ReturnTocar:
                if (self.nav_) self.nav_->requestReturnToPerformance();
                break;
            case ActionKind::ToggleInspector:
                self.toggleInspector();
                break;
            case ActionKind::CycleFocus:
                self.cycleFocus();
                break;
            case ActionKind::FineAdjust:
                self.stepFocusedParam(ctx->arg);
                break;
        }
    }

    void clearDetailPointers() {
        timing_ = nullptr;
        notice_ = nullptr;
        chorus_note_title_ = nullptr;
        chorus_note_body_ = nullptr;
        inspector_card_ = nullptr;
        insp_toggle_btn_ = nullptr;
        insp_toggle_lbl_ = nullptr;
        insp_compact_lbl_ = nullptr;
        insp_title_ = nullptr;
        insp_values_ = nullptr;
        insp_status_ = nullptr;
        insp_focus_btn_ = nullptr;
        insp_dec_btn_ = nullptr;
        insp_dec_lbl_ = nullptr;
        insp_inc_btn_ = nullptr;
        insp_inc_lbl_ = nullptr;
        notice_color_cached_ = 0;
        chorus_title_color_cached_ = 0;
        insp_status_color_cached_ = 0;
    }

    void selectModule(uint8_t mod) {
        const uint8_t target = static_cast<uint8_t>(mod % kFxModuleCount);
        if (active_module_ == target) return;
        active_module_ = target;
        if (nav_) nav_->setSubTab(ScreenId::Effects, active_module_);
        for (uint8_t i = 0; i < kFxModuleCount; ++i) {
            if (!sidebar_btns_[i]) continue;
            if (i == active_module_) lv_obj_add_state(sidebar_btns_[i], LV_STATE_CHECKED);
            else lv_obj_remove_state(sidebar_btns_[i], LV_STATE_CHECKED);
        }
        buildModulePanel();
    }

    void toggleInspector() {
        inspector_expanded_ = !inspector_expanded_;
        applyInspectorLayout();
        refreshInspector();
    }

    void cycleFocus() {
        const auto list = moduleParams(active_module_);
        if (list.count == 0) return;
        focus_idx_[active_module_] = static_cast<uint8_t>((focus_idx_[active_module_] + 1U) % list.count);
        refreshInspector();
    }

    const char* focusedParamId() const {
        const auto list = moduleParams(active_module_);
        const uint8_t idx = static_cast<uint8_t>(focus_idx_[active_module_] % list.count);
        return list.ids[idx];
    }

    void stepFocusedParam(int32_t delta) {
        if (!backend_) return;
        const char* pid = focusedParamId();
        if (!backend_->isParameterEditable(pid)) return;
        const ParameterDescriptor* d = ParameterRegistry::findById(pid);
        if (!d) return;

        if (d->type == ParameterValueType::Bool) {
            const bool next = !backend_->model().getBool(pid, false);
            backend_->requestParameterBool(pid, next);
        } else if (d->type == ParameterValueType::Enum) {
            int32_t cur = backend_->model().getInt(pid, 0);
            const int32_t min_v = static_cast<int32_t>(d->minimum);
            const int32_t max_v = (d->enum_count > 0) ? static_cast<int32_t>(d->enum_count - 1)
                                                      : static_cast<int32_t>(d->maximum);
            cur += (delta >= 0) ? 1 : -1;
            if (cur < min_v) cur = max_v;
            if (cur > max_v) cur = min_v;
            for (int32_t tries = 0; tries <= max_v - min_v && !backend_->isParameterValueSupported(pid, cur); ++tries) {
                cur += (delta >= 0) ? 1 : -1;
                if (cur < min_v) cur = max_v;
                if (cur > max_v) cur = min_v;
            }
            backend_->requestParameterInt(pid, cur);
        } else if (d->type == ParameterValueType::Int) {
            const int32_t step = std::max<int32_t>(1, static_cast<int32_t>(std::lround(d->step)));
            const int32_t cur = backend_->model().getInt(pid, static_cast<int32_t>(d->default_value));
            const int32_t next = std::clamp<int32_t>(cur + delta * step,
                                                     static_cast<int32_t>(d->minimum),
                                                     static_cast<int32_t>(d->maximum));
            backend_->requestParameterInt(pid, next);
        } else {
            const float step = (d->step > 0.0f) ? d->step : 1.0f;
            const float cur = backend_->model().getFloat(pid, d->default_value);
            const float next = std::clamp(cur + static_cast<float>(delta) * step, d->minimum, d->maximum);
            backend_->requestParameterFloat(pid, next);
        }

        // Propagate confirmed values (or demo values) to visible cards immediately.
        if (detail_panel_) {
            const uint32_t cnt = lv_obj_get_child_count(detail_panel_);
            for (uint32_t i = 0; i < cnt; ++i) {
                if (lv_obj_t* child = lv_obj_get_child(detail_panel_, static_cast<int32_t>(i))) {
                    lv_obj_send_event(child, LV_EVENT_REFRESH, nullptr);
                    const uint32_t sub = lv_obj_get_child_count(child);
                    for (uint32_t j = 0; j < sub; ++j) {
                        if (lv_obj_t* gc = lv_obj_get_child(child, static_cast<int32_t>(j))) {
                            lv_obj_send_event(gc, LV_EVENT_REFRESH, nullptr);
                        }
                    }
                }
            }
        }
        refresh(*backend_);
    }

    void buildInspector() {
        inspector_card_ = lv_obj_create(detail_panel_);
        if (!inspector_card_) return;
        applyPanelStyle(inspector_card_, true);
        lv_obj_set_style_pad_all(inspector_card_, 6, 0);
        lv_obj_remove_flag(inspector_card_, LV_OBJ_FLAG_SCROLLABLE);

        insp_title_buf_[0] = '\0';
        insp_values_buf_[0] = '\0';
        insp_status_buf_[0] = '\0';
        insp_compact_buf_[0] = '\0';

        insp_compact_lbl_ = createStaticLabel(inspector_card_, insp_compact_buf_, 132, 9, 342, palette::kTextPrimary);
        insp_title_ = createStaticLabel(inspector_card_, insp_title_buf_, 0, 0, 386, palette::kAccentPrimary);
        insp_status_ = createStaticLabel(inspector_card_, insp_status_buf_, 392, 0, 212, palette::kActive);
        if (insp_status_) {
            lv_obj_set_style_text_align(insp_status_, LV_TEXT_ALIGN_RIGHT, 0);
        }
        insp_values_ = createStaticLabel(inspector_card_, insp_values_buf_, 0, 22, 604, palette::kTextPrimary);

        insp_toggle_btn_ = createSidebarButton(inspector_card_, "AJUSTE FINO", 0, 1, 124, 36, false, &insp_toggle_lbl_);
        insp_focus_btn_ = createSidebarButton(inspector_card_, "PARAM >", 124, 54, 128, 36, false);
        insp_dec_btn_ = createSidebarButton(inspector_card_, "-", 482, 1, 58, 36, false, &insp_dec_lbl_);
        insp_inc_btn_ = createSidebarButton(inspector_card_, "+", 546, 1, 58, 36, false, &insp_inc_lbl_);

        if (insp_toggle_btn_) {
            attachLvContext<ActionCtx>(insp_toggle_btn_, ActionCtx{this, ActionKind::ToggleInspector, 0},
                                       onActionClicked, LV_EVENT_CLICKED);
        }
        if (insp_focus_btn_) {
            attachLvContext<ActionCtx>(insp_focus_btn_, ActionCtx{this, ActionKind::CycleFocus, 0},
                                       onActionClicked, LV_EVENT_CLICKED);
        }
        if (insp_dec_btn_) {
            attachLvContext<ActionCtx>(insp_dec_btn_, ActionCtx{this, ActionKind::FineAdjust, -1},
                                       onActionClicked, LV_EVENT_CLICKED);
        }
        if (insp_inc_btn_) {
            attachLvContext<ActionCtx>(insp_inc_btn_, ActionCtx{this, ActionKind::FineAdjust, 1},
                                       onActionClicked, LV_EVENT_CLICKED);
        }

        applyInspectorLayout();
    }

    void applyInspectorLayout() {
        if (!inspector_card_) return;
        if (!inspector_expanded_) {
            lv_obj_set_pos(inspector_card_, 8, 324);
            lv_obj_set_size(inspector_card_, 618, 52);

            setHiddenIfChanged(insp_compact_lbl_, false);
            setHiddenIfChanged(insp_title_, true);
            setHiddenIfChanged(insp_status_, true);
            setHiddenIfChanged(insp_values_, true);
            setHiddenIfChanged(insp_focus_btn_, true);

            if (insp_toggle_btn_) {
                lv_obj_set_pos(insp_toggle_btn_, 0, 1);
                lv_obj_set_size(insp_toggle_btn_, 124, 36);
            }
            if (insp_dec_btn_) {
                lv_obj_set_pos(insp_dec_btn_, 482, 1);
                lv_obj_set_size(insp_dec_btn_, 58, 36);
            }
            if (insp_inc_btn_) {
                lv_obj_set_pos(insp_inc_btn_, 546, 1);
                lv_obj_set_size(insp_inc_btn_, 58, 36);
            }
            setStaticButtonTextIfChanged(insp_toggle_lbl_, "AJUSTE FINO");
            setStaticButtonTextIfChanged(insp_dec_lbl_, "-");
            setStaticButtonTextIfChanged(insp_inc_lbl_, "+");
        } else {
            lv_obj_set_pos(inspector_card_, 8, 268);
            lv_obj_set_size(inspector_card_, 618, 108);

            setHiddenIfChanged(insp_compact_lbl_, true);
            setHiddenIfChanged(insp_title_, false);
            setHiddenIfChanged(insp_status_, false);
            setHiddenIfChanged(insp_values_, false);
            setHiddenIfChanged(insp_focus_btn_, false);

            if (insp_toggle_btn_) {
                lv_obj_set_pos(insp_toggle_btn_, 0, 54);
                lv_obj_set_size(insp_toggle_btn_, 116, 36);
            }
            if (insp_focus_btn_) {
                lv_obj_set_pos(insp_focus_btn_, 124, 54);
                lv_obj_set_size(insp_focus_btn_, 128, 36);
            }
            if (insp_dec_btn_) {
                lv_obj_set_pos(insp_dec_btn_, 260, 54);
                lv_obj_set_size(insp_dec_btn_, 96, 36);
            }
            if (insp_inc_btn_) {
                lv_obj_set_pos(insp_inc_btn_, 364, 54);
                lv_obj_set_size(insp_inc_btn_, 96, 36);
            }
            setStaticButtonTextIfChanged(insp_toggle_lbl_, "FECHAR");
            setStaticButtonTextIfChanged(insp_dec_lbl_, "- FINO");
            setStaticButtonTextIfChanged(insp_inc_lbl_, "+ FINO");
        }
    }

    void refreshInspector() {
        if (!backend_ || !insp_title_ || !insp_values_ || !insp_status_ || !insp_compact_lbl_) return;
        const auto list = moduleParams(active_module_);
        const uint8_t idx = static_cast<uint8_t>(focus_idx_[active_module_] % list.count);
        const char* pid = list.ids[idx];
        const ParameterDescriptor* d = ParameterRegistry::findById(pid);
        if (!d) return;

        ParameterValue pv{};
        backend_->model().getValue(pid, pv);
        char vbuf[32];
        ParameterRegistry::formatValue(*d, pv, vbuf, sizeof(vbuf));

        const FeatureStatus fs = backend_->parameterStatus(pid);
        const bool editable = backend_->isParameterEditable(pid);

        const char* st_text = "CONFIRMADO";
        uint32_t st_color = palette::kActive;
        if (backend_->lastCommandParameterId() && std::strcmp(backend_->lastCommandParameterId(), pid) == 0) {
            const UiCommandState cst = backend_->lastCommandState();
            st_text = commandStateTextPt(cst);
            if (cst == UiCommandState::Requested) st_color = palette::kWarning;
            else if (cst == UiCommandState::Rejected || cst == UiCommandState::Unsupported) st_color = palette::kErrorPanic;
        } else if (backend_->isDemoMode()) {
            st_text = "MODO DEMO";
            st_color = palette::kAccentPrimary;
        } else if (fs != FeatureStatus::Available) {
            st_text = featureStatusLabel(fs);
            st_color = featureStatusColor(fs);
        } else if (!editable) {
            if (std::strcmp(pid, "fx.delay.time_ms") == 0) st_text = "SYNC ATIVO";
            else if (std::strcmp(pid, "fx.chorus.depth") == 0) st_text = "CHORUS OFF";
            else st_text = "BLOQUEADO";
            st_color = palette::kWarning;
        }

        if (!inspector_expanded_) {
            char cbuf[96];
            if (!editable) {
                std::snprintf(cbuf, sizeof(cbuf), "%s: %s (%s)", d->name, vbuf, st_text);
            } else {
                std::snprintf(cbuf, sizeof(cbuf), "%u/%u %s: %s",
                              unsigned(idx + 1U), unsigned(list.count), d->name, vbuf);
            }
            updateStaticBufLabel(insp_compact_lbl_, insp_compact_buf_, sizeof(insp_compact_buf_), cbuf);
        } else {
            char tbuf[80];
            std::snprintf(tbuf, sizeof(tbuf), "AJUSTE FINO: %s (%u/%u)",
                          d->name, unsigned(idx + 1U), unsigned(list.count));
            updateStaticBufLabel(insp_title_, insp_title_buf_, sizeof(insp_title_buf_), tbuf);

            setTextColorIfChanged(insp_status_, insp_status_color_cached_, st_color);
            updateStaticBufLabel(insp_status_, insp_status_buf_, sizeof(insp_status_buf_), st_text);

            char val_line[128];
            if (std::strcmp(pid, "fx.delay.time_ms") == 0) {
                IUiBackend::DelayStatus ds{};
                if (backend_->delayStatus(ds)) {
                    std::snprintf(val_line, sizeof(val_line),
                                  "Atual: %s | Alvo: %.1f ms | DSP: %.1f ms | %s",
                                  vbuf, double(ds.target_ms), double(ds.applied_ms),
                                  ds.sync ? "Sync travado" : "Tempo livre");
                } else {
                    std::snprintf(val_line, sizeof(val_line),
                                  "Atual: %s | Passo fino: %.4g %s",
                                  vbuf, double(d->step), (d->unit && d->unit[0]) ? d->unit : "");
                }
            } else if (active_module_ == 3) {
                std::snprintf(val_line, sizeof(val_line),
                              "Atual: %s | Passo fino: %.4g %s | Drive e roteamento: futuro",
                              vbuf, double(d->step), (d->unit && d->unit[0]) ? d->unit : "opcao");
            } else {
                std::snprintf(val_line, sizeof(val_line),
                              "Atual: %s | Passo fino: %.4g %s | Cadeia fixa",
                              vbuf, double(d->step), (d->unit && d->unit[0]) ? d->unit : "opcao");
            }
            updateStaticBufLabel(insp_values_, insp_values_buf_, sizeof(insp_values_buf_), val_line);
        }

        setDisabledIfChanged(insp_dec_btn_, !editable);
        setDisabledIfChanged(insp_inc_btn_, !editable);
    }

    void buildModulePanel() {
        if (!detail_panel_ || !backend_) return;
        clearDetailPointers();
        lv_obj_clean(detail_panel_);
        timing_buf_[0] = '\0';
        notice_buf_[0] = '\0';
        chorus_note_title_buf_[0] = '\0';
        chorus_note_body_buf_[0] = '\0';

        if (active_module_ == 0) {
            createStaticLabel(detail_panel_, "DELAY", 8, 2, 110, palette::kAccentPrimary);
            timing_ = createStaticLabel(detail_panel_, timing_buf_, 122, 2, 504, palette::kTextPrimary);
            notice_ = createStaticLabel(detail_panel_, notice_buf_, 8, 21, 618, palette::kTextSecondary);

            createParameterCard(detail_panel_, *backend_, "fx.delay.mode", 8, 44, 202, 104,
                                PresentationLevel::StageStacked);
            createParameterCard(detail_panel_, *backend_, "fx.delay.division", 216, 44, 202, 104,
                                PresentationLevel::StageStacked);
            createParameterCard(detail_panel_, *backend_, "fx.delay.time_ms", 424, 44, 202, 104,
                                PresentationLevel::StageStacked);

            createParameterCard(detail_panel_, *backend_, "fx.delay.feedback", 8, 154, 202, 108,
                                PresentationLevel::StageStacked);
            createParameterCard(detail_panel_, *backend_, "fx.delay.mix", 216, 154, 202, 108,
                                PresentationLevel::StageStacked);
            createInfoBox(detail_panel_, "BYPASS",
                          "Delay sem bypass\nindividual. Mix 0%\nmantem macros.",
                          424, 154, 202, 108, palette::kWarning);
        } else if (active_module_ == 1) {
            createStaticLabel(detail_panel_, "REVERB", 8, 2, 110, palette::kAccentPrimary);
            timing_ = createStaticLabel(detail_panel_, timing_buf_, 122, 2, 504, palette::kTextPrimary);
            notice_ = createStaticLabel(detail_panel_, notice_buf_, 8, 21, 618, palette::kTextSecondary);

            createParameterCard(detail_panel_, *backend_, "fx.reverb.size", 8, 44, 306, 148,
                                PresentationLevel::StageStacked);
            createParameterCard(detail_panel_, *backend_, "fx.reverb.mix", 320, 44, 306, 148,
                                PresentationLevel::StageStacked);
            createInfoBox(detail_panel_, "NOTA",
                          "Sem bypass individual | Mix 0% nao desativa macros. Use FX Bypass em Master.",
                          8, 200, 618, 58, palette::kWarning);
        } else if (active_module_ == 2) {
            createStaticLabel(detail_panel_, "CHORUS", 8, 2, 110, palette::kAccentPrimary);
            timing_ = createStaticLabel(detail_panel_, timing_buf_, 122, 2, 504, palette::kTextPrimary);
            notice_ = createStaticLabel(detail_panel_, notice_buf_, 8, 21, 618, palette::kTextSecondary);

            createParameterCard(detail_panel_, *backend_, "fx.chorus.mode", 8, 44, 306, 148,
                                PresentationLevel::StageStacked);
            createParameterCard(detail_panel_, *backend_, "fx.chorus.depth", 320, 44, 306, 148,
                                PresentationLevel::StageStacked);
            createInfoBox(detail_panel_, "", "", 8, 200, 618, 58, palette::kActive,
                          &chorus_note_title_, &chorus_note_body_);
        } else {
            createStaticLabel(detail_panel_, "MASTER & EQ", 8, 2, 140, palette::kAccentPrimary);
            timing_ = createStaticLabel(detail_panel_, timing_buf_, 152, 2, 474, palette::kTextPrimary);
            notice_ = createStaticLabel(detail_panel_, notice_buf_, 8, 21, 618, palette::kTextSecondary);

            createParameterCard(detail_panel_, *backend_, "fx.bypass_all", 8, 44, 202, 104,
                                PresentationLevel::StageStacked);
            createParameterCard(detail_panel_, *backend_, "perf.master_volume", 216, 44, 202, 104,
                                PresentationLevel::StageStacked);
            createParameterCard(detail_panel_, *backend_, "fx.master_tone", 424, 44, 202, 104,
                                PresentationLevel::StageStacked);

            createParameterCard(detail_panel_, *backend_, "fx.eq.low_db", 8, 154, 202, 108,
                                PresentationLevel::StageStacked);
            createParameterCard(detail_panel_, *backend_, "fx.eq.mid_db", 216, 154, 202, 108,
                                PresentationLevel::StageStacked);
            createParameterCard(detail_panel_, *backend_, "fx.eq.high_db", 424, 154, 202, 108,
                                PresentationLevel::StageStacked);
        }

        buildInspector();
        refresh(*backend_);
    }

    IUiBackend*          backend_ = nullptr;
    UiNavigationManager* nav_ = nullptr;
    lv_obj_t*            sidebar_ = nullptr;
    lv_obj_t*            detail_panel_ = nullptr;
    lv_obj_t*            sidebar_btns_[kFxModuleCount]{};
    lv_obj_t*            chain_status_lbl_ = nullptr;
    lv_obj_t*            timing_ = nullptr;
    lv_obj_t*            notice_ = nullptr;
    lv_obj_t*            chorus_note_title_ = nullptr;
    lv_obj_t*            chorus_note_body_ = nullptr;
    lv_obj_t*            inspector_card_ = nullptr;
    lv_obj_t*            insp_toggle_btn_ = nullptr;
    lv_obj_t*            insp_toggle_lbl_ = nullptr;
    lv_obj_t*            insp_compact_lbl_ = nullptr;
    lv_obj_t*            insp_title_ = nullptr;
    lv_obj_t*            insp_values_ = nullptr;
    lv_obj_t*            insp_status_ = nullptr;
    lv_obj_t*            insp_focus_btn_ = nullptr;
    lv_obj_t*            insp_dec_btn_ = nullptr;
    lv_obj_t*            insp_dec_lbl_ = nullptr;
    lv_obj_t*            insp_inc_btn_ = nullptr;
    lv_obj_t*            insp_inc_lbl_ = nullptr;
    uint8_t              active_module_ = 0;
    uint8_t              focus_idx_[kFxModuleCount]{};
    bool                 inspector_expanded_ = false;
    bool                 chain_bypassed_known_ = false;
    bool                 chain_bypassed_cached_ = false;
    uint32_t             notice_color_cached_ = 0;
    uint32_t             chorus_title_color_cached_ = 0;
    uint32_t             insp_status_color_cached_ = 0;
    char                 timing_buf_[96]{};
    char                 notice_buf_[96]{};
    char                 chorus_note_title_buf_[48]{};
    char                 chorus_note_body_buf_[128]{};
    char                 insp_compact_buf_[96]{};
    char                 insp_title_buf_[80]{};
    char                 insp_values_buf_[128]{};
    char                 insp_status_buf_[48]{};
};

} // namespace

ScreenView* createEffectsScreen() { return new EffectsScreen(); }

} // namespace smk::p4::ui
