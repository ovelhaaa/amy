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

const char* commandStateText(UiCommandState state) {
    switch (state) {
        case UiCommandState::Confirmed:   return "CONFIRMED";
        case UiCommandState::Requested:   return "WAIT OWNER";
        case UiCommandState::Rejected:    return "REJECTED";
        case UiCommandState::Unsupported: return "UNSUPPORTED";
        default:                          return "UNKNOWN";
    }
}

void updateStaticBufLabel(lv_obj_t* label, char* buf, size_t cap, const char* text) {
    if (!label || !buf || cap == 0 || !text) return;
    if (std::strcmp(buf, text) == 0) return;
    std::snprintf(buf, cap, "%s", text);
    lv_label_set_text_static(label, buf);
    lv_obj_invalidate(label);
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

        lv_obj_t* side_title = createStaticLabel(sidebar_, "FX MODULES", 8, 6, 136, palette::kTextSecondary);
        (void)side_title;

        for (uint8_t i = 0; i < kFxModuleCount; ++i) {
            sidebar_btns_[i] = createSidebarButton(sidebar_, kModuleLabels[i], 6, 26 + static_cast<int32_t>(i) * 56, 140, 50,
                                                   i == active_module_);
            if (sidebar_btns_[i]) {
                attachLvContext<ActionCtx>(sidebar_btns_[i], ActionCtx{this, ActionKind::SelectModule, int32_t(i)},
                                           onActionClicked, LV_EVENT_CLICKED);
            }
        }

        createStaticLabel(sidebar_, "CHAIN (FIXED):", 6, 254, 140, palette::kTextSecondary);
        createStaticLabel(sidebar_, "CH>DLY>REV>EQ", 6, 274, 140, palette::kTextPrimary);
        chain_status_lbl_ = createStaticLabel(sidebar_, "CHAIN: ACTIVE", 6, 296, 140, palette::kActive);
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
            lv_label_set_text_static(chain_status_lbl_, effective_bypass ? "CHAIN: BYPASSED" : "CHAIN: ACTIVE");
            lv_obj_set_style_text_color(
                chain_status_lbl_, lv_color_hex(effective_bypass ? palette::kWarning : palette::kActive), 0);
        }

        const int bpm = backend.model().getInt("perf.bpm", 120);
        if (active_module_ == 0) {
            if (timing_) {
                if (has_ds) {
                    char timing[96];
                    if (status.sync) {
                        std::snprintf(timing, sizeof(timing), "%d BPM | TARGET %.2f ms | DSP %.2f ms",
                                      bpm, double(status.target_ms), double(status.applied_ms));
                    } else {
                        std::snprintf(timing, sizeof(timing), "%d BPM | DSP %.2f ms",
                                      bpm, double(status.applied_ms));
                    }
                    updateStaticBufLabel(timing_, timing_buf_, sizeof(timing_buf_), timing);
                } else {
                    updateStaticBufLabel(timing_, timing_buf_, sizeof(timing_buf_),
                                         backend.isDemoMode() ? "DEMO RAM | NO OWNER DELAY STATUS" : "WAIT SNAPSHOT");
                }
            }
            if (notice_) {
                const char* msg = "BYPASS INDIV: N/A | MIX 0% != BYPASS M3/M7";
                if (effective_bypass) {
                    msg = "FX BYPASSED";
                } else if (has_ds && status.out_of_range) {
                    msg = "SYNC OUT OF RANGE | HOLDING SAFE TIME | TIME MACROS PAUSED";
                } else if ((has_ds && status.sync) || backend.model().getInt("fx.delay.mode", 0) == 1) {
                    msg = "SYNC ATIVO | MANUAL TIME TRAVADO | TIME MACROS PAUSED";
                }
                updateStaticBufLabel(notice_, notice_buf_, sizeof(notice_buf_), msg);
            }
        } else if (active_module_ == 1) {
            updateStaticBufLabel(timing_, timing_buf_, sizeof(timing_buf_),
                                 effective_bypass ? "GLOBAL BYPASS ON" : "FIXED CHAIN: CHOR>DLY>REV>EQ");
            updateStaticBufLabel(notice_, notice_buf_, sizeof(notice_buf_),
                                 effective_bypass ? "FX BYPASSED GLOBALLY (MASTER)"
                                                  : "REVERB SEM BYPASS INDIVIDUAL | MIX 0% NAO ANULA M6/M7");
        } else if (active_module_ == 2) {
            const int32_t mode = backend.model().getInt("fx.chorus.mode", 0);
            updateStaticBufLabel(timing_, timing_buf_, sizeof(timing_buf_),
                                 mode == 0 ? "CHORUS: OFF REAL (LVL 0.0)" : "CHORUS: ATIVO NO BUS AMY");
            updateStaticBufLabel(notice_, notice_buf_, sizeof(notice_buf_),
                                 effective_bypass ? "FX BYPASSED GLOBALLY (MASTER)"
                                                  : (mode == 0 ? "MODO OFF DESLIGA CHORUS REAL (DEPTH INATIVO)"
                                                               : "MODO ATIVO | DEPTH DISPONIVEL EM CONFIRMACAO"));
        } else if (active_module_ == 3) {
            updateStaticBufLabel(timing_, timing_buf_, sizeof(timing_buf_),
                                 effective_bypass ? "BYPASS ALL: ON" : "CHAIN: CHOR>DLY>REV>EQ");
            updateStaticBufLabel(notice_, notice_buf_, sizeof(notice_buf_),
                                 "MASTER VOL + TILT TONE + EQ 3 BANDAS + BYPASS ALL | PLANNED: DRIVE / ROUTING");
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
        CycleFocus,
        FineAdjust
    };

    struct ActionCtx {
        EffectsScreen* self = nullptr;
        ActionKind kind = ActionKind::SelectModule;
        int32_t arg = 0;
    };

    static lv_obj_t* createStaticLabel(lv_obj_t* parent, const char* static_text,
                                       int32_t x, int32_t y, int32_t w, uint32_t color_hex) {
        lv_obj_t* lbl = lv_label_create(parent);
        if (!lbl) return nullptr;
        lv_obj_set_pos(lbl, x, y);
        lv_obj_set_size(lbl, w, lv_font_get_line_height(&lv_font_montserrat_14) + 4);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(color_hex), 0);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_MODE_CLIP);
        lv_label_set_text_static(lbl, static_text ? static_text : "");
        return lbl;
    }

    static lv_obj_t* createSidebarButton(lv_obj_t* parent, const char* static_label,
                                         int32_t x, int32_t y, int32_t w, int32_t h, bool checked) {
        lv_obj_t* btn = lv_button_create(parent);
        if (!btn) return nullptr;
        lv_obj_remove_style_all(btn);
        lv_obj_add_style(btn, &theme().button, 0);
        lv_obj_add_style(btn, &theme().button_active, LV_STATE_CHECKED);
        lv_obj_set_pos(btn, x, y);
        lv_obj_set_size(btn, w, h);
        lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        if (checked) lv_obj_add_state(btn, LV_STATE_CHECKED);

        if (lv_obj_t* lbl = lv_label_create(btn)) {
            lv_label_set_text_static(lbl, static_label ? static_label : "");
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
            lv_obj_center(lbl);
        }
        return btn;
    }

    static lv_obj_t* createInfoBox(lv_obj_t* parent, const char* title, const char* body,
                                   int32_t x, int32_t y, int32_t w, int32_t h,
                                   uint32_t title_color_hex) {
        lv_obj_t* card = lv_obj_create(parent);
        if (!card) return nullptr;
        applyPanelStyle(card, false);
        lv_obj_set_pos(card, x, y);
        lv_obj_set_size(card, w, h);

        createStaticLabel(card, title, 2, 0, w - 16, title_color_hex);
        if (lv_obj_t* lbl = lv_label_create(card)) {
            lv_label_set_long_mode(lbl, LV_LABEL_LONG_MODE_WRAP);
            lv_obj_set_pos(lbl, 2, 22);
            lv_obj_set_size(lbl, w - 16, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(lbl, lv_color_hex(palette::kTextSecondary), 0);
            lv_label_set_text_static(lbl, body ? body : "");
        }
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
        insp_title_ = nullptr;
        insp_values_ = nullptr;
        insp_status_ = nullptr;
        insp_dec_btn_ = nullptr;
        insp_inc_btn_ = nullptr;
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
        lv_obj_t* card = lv_obj_create(detail_panel_);
        if (!card) return;
        applyPanelStyle(card, true);
        lv_obj_set_pos(card, 8, 272);
        lv_obj_set_size(card, 618, 104);

        insp_title_buf_[0] = '\0';
        insp_values_buf_[0] = '\0';
        insp_status_buf_[0] = '\0';
        insp_title_ = createStaticLabel(card, insp_title_buf_, 2, 0, 600, palette::kAccentPrimary);
        insp_values_ = createStaticLabel(card, insp_values_buf_, 2, 20, 600, palette::kTextPrimary);

        lv_obj_t* focus_btn = createSidebarButton(card, "FOCUS >", 2, 48, 96, 36, false);
        insp_dec_btn_ = createSidebarButton(card, "- FINE", 104, 48, 82, 36, false);
        insp_inc_btn_ = createSidebarButton(card, "+ FINE", 192, 48, 82, 36, false);

        if (focus_btn) {
            attachLvContext<ActionCtx>(focus_btn, ActionCtx{this, ActionKind::CycleFocus, 0},
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

        insp_status_ = createStaticLabel(card, insp_status_buf_, 280, 56, 324, palette::kTextSecondary);
    }

    void refreshInspector() {
        if (!backend_ || !insp_title_ || !insp_values_ || !insp_status_) return;
        const char* pid = focusedParamId();
        const ParameterDescriptor* d = ParameterRegistry::findById(pid);
        if (!d) return;

        char tbuf[80];
        std::snprintf(tbuf, sizeof(tbuf), "INSPECTOR: %s (%s)", d->name, d->id);
        updateStaticBufLabel(insp_title_, insp_title_buf_, sizeof(insp_title_buf_), tbuf);

        ParameterValue pv{};
        backend_->model().getValue(pid, pv);
        char vbuf[32];
        ParameterRegistry::formatValue(*d, pv, vbuf, sizeof(vbuf));

        const FeatureStatus fs = backend_->parameterStatus(pid);
        const bool editable = backend_->isParameterEditable(pid);

        char val_line[120];
        if (std::strcmp(pid, "fx.delay.time_ms") == 0) {
            IUiBackend::DelayStatus ds{};
            if (backend_->delayStatus(ds)) {
                std::snprintf(val_line, sizeof(val_line),
                              "CONFIRMADO: %s | TARGET: %.1f ms | DSP: %.1f ms | %s",
                              vbuf, double(ds.target_ms), double(ds.applied_ms),
                              ds.sync ? "SYNC LOCK" : "FREE MS");
            } else {
                std::snprintf(val_line, sizeof(val_line),
                              "VALOR: %s | UNIT: %s | %s",
                              vbuf, (d->unit && d->unit[0]) ? d->unit : "ENUM/BOOL",
                              backend_->isDemoMode() ? "DEMO RAM" : "OWNER");
            }
        } else {
            std::snprintf(val_line, sizeof(val_line),
                          "VALOR OWNER: %s | UNIT: %s | %s",
                          vbuf, (d->unit && d->unit[0]) ? d->unit : "ENUM/BOOL",
                          backend_->isDemoMode() ? "DEMO RAM (NO AMY)" : "CONFIRMED SNAPSHOT");
        }
        updateStaticBufLabel(insp_values_, insp_values_buf_, sizeof(insp_values_buf_), val_line);

        const char* st_text = "CONFIRMED";
        if (backend_->lastCommandParameterId() && std::strcmp(backend_->lastCommandParameterId(), pid) == 0) {
            st_text = commandStateText(backend_->lastCommandState());
        } else if (backend_->isDemoMode()) {
            st_text = "DEMO RAM";
        } else if (fs != FeatureStatus::Available) {
            st_text = featureStatusLabel(fs);
        } else if (!editable) {
            st_text = (std::strcmp(pid, "fx.delay.time_ms") == 0) ? "SYNC LOCKED" : "LOCKED";
        }

        char sbuf[96];
        if (active_module_ == 3) {
            std::snprintf(sbuf, sizeof(sbuf), "ST: %s | PLAN: fx.drive/routing", st_text);
        } else {
            std::snprintf(sbuf, sizeof(sbuf), "ST: %s | FINE STEP: %.4g %s",
                          st_text, double(d->step), (d->unit && d->unit[0]) ? d->unit : "");
        }
        updateStaticBufLabel(insp_status_, insp_status_buf_, sizeof(insp_status_buf_), sbuf);

        for (lv_obj_t* btn : {insp_dec_btn_, insp_inc_btn_}) {
            if (!btn) continue;
            if (editable) lv_obj_remove_state(btn, LV_STATE_DISABLED);
            else lv_obj_add_state(btn, LV_STATE_DISABLED);
        }
    }

    void buildModulePanel() {
        if (!detail_panel_ || !backend_) return;
        clearDetailPointers();
        lv_obj_clean(detail_panel_);
        timing_buf_[0] = '\0';
        notice_buf_[0] = '\0';

        if (active_module_ == 0) {
            createStaticLabel(detail_panel_, "FX > DELAY", 8, 2, 120, palette::kAccentPrimary);
            timing_ = createStaticLabel(detail_panel_, timing_buf_, 132, 2, 494, palette::kTextPrimary);
            notice_ = createStaticLabel(detail_panel_, notice_buf_, 8, 21, 618, palette::kWarning);

            createParameterCard(detail_panel_, *backend_, "fx.delay.mode", 8, 44, 202, 104);
            createParameterCard(detail_panel_, *backend_, "fx.delay.division", 216, 44, 202, 104);
            createParameterCard(detail_panel_, *backend_, "fx.delay.time_ms", 424, 44, 202, 104);

            createParameterCard(detail_panel_, *backend_, "fx.delay.feedback", 8, 154, 202, 112);
            createParameterCard(detail_panel_, *backend_, "fx.delay.mix", 216, 154, 202, 112);
            createInfoBox(detail_panel_, "BYPASS INDIV: N/A",
                          "Sem ON/OFF indiv. no\nFxConfig/IUiBackend.\nMix=0% != M3/M7.",
                          424, 154, 202, 112, palette::kWarning);
        } else if (active_module_ == 1) {
            createStaticLabel(detail_panel_, "FX > REVERB", 8, 2, 160, palette::kAccentPrimary);
            timing_ = createStaticLabel(detail_panel_, timing_buf_, 172, 2, 454, palette::kTextPrimary);
            notice_ = createStaticLabel(detail_panel_, notice_buf_, 8, 21, 618, palette::kWarning);

            createParameterCard(detail_panel_, *backend_, "fx.reverb.size", 8, 44, 306, 112);
            createParameterCard(detail_panel_, *backend_, "fx.reverb.mix", 320, 44, 306, 112);
            createInfoBox(detail_panel_, "BYPASS INDIV: N/A",
                          "Nao ha comando ON/OFF individual de Reverb em FxConfig/IUiBackend.\n"
                          "Reduzir Mix para 0% NAO equivale a bypass nem neutraliza macros M6/M7.",
                          8, 162, 618, 104, palette::kWarning);
        } else if (active_module_ == 2) {
            createStaticLabel(detail_panel_, "FX > CHORUS", 8, 2, 160, palette::kAccentPrimary);
            timing_ = createStaticLabel(detail_panel_, timing_buf_, 172, 2, 454, palette::kTextPrimary);
            notice_ = createStaticLabel(detail_panel_, notice_buf_, 8, 21, 618, palette::kWarning);

            createParameterCard(detail_panel_, *backend_, "fx.chorus.mode", 8, 44, 306, 112);
            createParameterCard(detail_panel_, *backend_, "fx.chorus.depth", 320, 44, 306, 112);
            createInfoBox(detail_panel_, "CHORUS OFF REAL (AUDIO OWNER)",
                          "Selecionar OFF em fx.chorus.mode aplica chorus_level=0.0 em applyBusFx().\n"
                          "Modos ativos: CLASSIC, JUNO I, JUNO II, ENSEMBLE, VIBRATO.",
                          8, 162, 618, 104, palette::kActive);
        } else {
            createStaticLabel(detail_panel_, "FX > MASTER & 3-BAND EQ", 8, 2, 224, palette::kAccentPrimary);
            timing_ = createStaticLabel(detail_panel_, timing_buf_, 238, 2, 388, palette::kTextPrimary);
            notice_ = createStaticLabel(detail_panel_, notice_buf_, 8, 21, 618, palette::kTextSecondary);

            createParameterCard(detail_panel_, *backend_, "fx.bypass_all", 8, 44, 202, 104);
            createParameterCard(detail_panel_, *backend_, "perf.master_volume", 216, 44, 202, 104);
            createParameterCard(detail_panel_, *backend_, "fx.master_tone", 424, 44, 202, 104);

            createParameterCard(detail_panel_, *backend_, "fx.eq.low_db", 8, 154, 202, 112);
            createParameterCard(detail_panel_, *backend_, "fx.eq.mid_db", 216, 154, 202, 112);
            createParameterCard(detail_panel_, *backend_, "fx.eq.high_db", 424, 154, 202, 112);
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
    lv_obj_t*            insp_title_ = nullptr;
    lv_obj_t*            insp_values_ = nullptr;
    lv_obj_t*            insp_status_ = nullptr;
    lv_obj_t*            insp_dec_btn_ = nullptr;
    lv_obj_t*            insp_inc_btn_ = nullptr;
    uint8_t              active_module_ = 0;
    uint8_t              focus_idx_[kFxModuleCount]{};
    bool                 chain_bypassed_known_ = false;
    bool                 chain_bypassed_cached_ = false;
    char                 timing_buf_[96]{};
    char                 notice_buf_[96]{};
    char                 insp_title_buf_[80]{};
    char                 insp_values_buf_[120]{};
    char                 insp_status_buf_[96]{};
};

} // namespace

ScreenView* createEffectsScreen() { return new EffectsScreen(); }

} // namespace smk::p4::ui
