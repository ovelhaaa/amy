#include "ui_screens.h"
#include "ui_widgets.h"
#include "routing_editor.h"
#include <cstdio>
#include <cstring>
#include <memory>

namespace smk::p4::ui {

namespace {

const char* sceneOpResultShort(scene::RecallPhase phase) {
    switch (phase) {
        case scene::RecallPhase::WaitingBoundary: return "PENDING";
        case scene::RecallPhase::WaitingPatch:    return "WAIT PATCH";
        case scene::RecallPhase::Failed:          return "FAILED";
        case scene::RecallPhase::Cancelled:       return "CANCELLED";
        case scene::RecallPhase::PartialApply:    return "PARTIAL";
        default:                                  return nullptr;
    }
}

const char* scenePhaseShort(scene::RecallPhase phase, const scene::Slot* slot) {
    switch (phase) {
        case scene::RecallPhase::WaitingBoundary: return "PENDING BAR";
        case scene::RecallPhase::WaitingPatch:    return "WAIT PATCH";
        case scene::RecallPhase::Confirmed:       return "ACTIVE";
        case scene::RecallPhase::Failed:          return "FAILED";
        case scene::RecallPhase::Cancelled:       return "CANCELLED";
        case scene::RecallPhase::PartialApply:    return "PARTIAL";
        case scene::RecallPhase::Idle:
        default:
            if (!slot || slot->state == seq::SlotState::Empty) return "EMPTY";
            if (slot->state == seq::SlotState::Corrupted) return "CORRUPT";
            if (slot->state == seq::SlotState::Incompatible) return "INCOMP";
            return "SAVED";
    }
}

uint32_t sceneStateColor(const char* label) {
    if (!label) return palette::kPlannedBadge;
    if (std::strcmp(label, "ACTIVE") == 0) return palette::kActive;
    if (std::strncmp(label, "PENDING", 7) == 0 || std::strcmp(label, "WAIT PATCH") == 0) return palette::kAccentPrimary;
    if (std::strcmp(label, "FAILED") == 0 || std::strcmp(label, "CORRUPT") == 0) return palette::kErrorPanic;
    if (std::strcmp(label, "PARTIAL") == 0 || std::strcmp(label, "INCOMP") == 0) return palette::kWarning;
    if (std::strcmp(label, "SAVED") == 0) return palette::kPanelHighlight;
    return palette::kPlannedBadge;
}

uint32_t sceneBadgeTextColor(const char* label) {
    if (!label) return palette::kTextPrimary;
    if (std::strcmp(label, "ACTIVE") == 0 ||
        std::strncmp(label, "PENDING", 7) == 0 ||
        std::strcmp(label, "WAIT PATCH") == 0 ||
        std::strcmp(label, "PARTIAL") == 0 ||
        std::strcmp(label, "INCOMP") == 0) {
        return palette::kBackground;
    }
    return palette::kTextPrimary;
}

lv_obj_t* createPill(lv_obj_t* parent, const char* text, int32_t x, int32_t y,
                     int32_t w, int32_t h, uint32_t bg_hex, uint32_t txt_hex = palette::kTextPrimary) {
    lv_obj_t* box = lv_obj_create(parent);
    if (!box) return nullptr;
    lv_obj_remove_style_all(box);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, w, h);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(box, lv_color_hex(bg_hex), 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(palette::kBorder), 0);
    lv_obj_set_style_radius(box, 4, 0);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    if (lv_obj_t* lbl = lv_label_create(box)) {
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(lbl, w - 4);
        lv_label_set_text(lbl, text ? text : "");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(txt_hex), 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(lbl);
    }
    return box;
}

void setLabelIfChanged(lv_obj_t* lbl, const char* text) {
    if (!lbl || !text) return;
    const char* cur = lv_label_get_text(lbl);
    if (!cur || std::strcmp(cur, text) != 0) lv_label_set_text(lbl, text);
}

class PerformanceScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::Performance; }
    bool hasBlockingConfirmation() const override { return confirm_dialog_.isOpen(); }

    bool build(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav) override {
        parent_ = parent;
        backend_ = &backend;
        confirm_dialog_.close(false);
        last_demo_panic_count_ = backend.model().diagnostics().demo_panic_count;
        panic_active_ = false;
        legacy_ = nullptr;
        confirm_ = nullptr;
        oct_lbl_ = status_lbl_ = kb_ = nullptr;
        patch_lbl_ = scene_lbl_ = recall_op_lbl_ = arp_pill_ = seq_pill_ = fx_pill_ = harm_pill_ = nullptr;
        scenes_notice_lbl_ = scenes_domains_lbl_ = nullptr;
        for (auto& q : quick_) q = nullptr;
        for (auto& c : scene_cards_) c = {};
        for (auto& l : lock_btns_) l = nullptr;

        const uint8_t view_mode = nav.subTab(ScreenId::Performance);
        if (view_mode == 3) {
            routing_ = std::make_unique<RoutingEditor>();
            routing_root_ = lv_obj_create(parent);
            lv_obj_remove_style_all(routing_root_);
            lv_obj_set_size(routing_root_, lv_pct(100), lv_pct(100));
            lv_obj_remove_flag(routing_root_, LV_OBJ_FLAG_SCROLLABLE);
            return routing_->build(routing_root_, backend, nav);
        }
        if (view_mode == 2) return buildLegacy(parent, backend, nav);

        // 1. Musical Context Banner (y = 2..58, full 800px width, no sidebar)
        lv_obj_t* top = lv_obj_create(parent);
        if (top) {
            applyPanelStyle(top, false);
            lv_obj_set_pos(top, 8, 2);
            lv_obj_set_size(top, 784, 56);
            lv_obj_set_style_pad_all(top, 2, 0);
            lv_obj_remove_flag(top, LV_OBJ_FLAG_SCROLLABLE);

            patch_lbl_ = createLabel(top, "", 6, 1, 322, &lv_font_montserrat_14, palette::kAccentPrimary);
            if (patch_lbl_) {
                lv_label_set_long_mode(patch_lbl_, LV_LABEL_LONG_MODE_DOTS);
                lv_obj_add_flag(patch_lbl_, LV_OBJ_FLAG_CLICKABLE);
                attachLvContext<UiNavigationManager*>(patch_lbl_, &nav, [](lv_event_t* e) {
                    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                    auto* n = *static_cast<UiNavigationManager**>(lv_event_get_user_data(e));
                    if (n) n->requestNavigateTo(ScreenId::PatchBrowser);
                }, LV_EVENT_CLICKED);
            }
            scene_lbl_ = createLabel(top, "", 6, 18, 322, &lv_font_montserrat_14, palette::kTextPrimary);
            if (scene_lbl_) {
                lv_label_set_long_mode(scene_lbl_, LV_LABEL_LONG_MODE_DOTS);
                lv_obj_add_flag(scene_lbl_, LV_OBJ_FLAG_CLICKABLE);
                attachLvContext<UiNavigationManager*>(scene_lbl_, &nav, [](lv_event_t* e) {
                    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                    auto* n = *static_cast<UiNavigationManager**>(lv_event_get_user_data(e));
                    if (n) n->requestNavigateTo(ScreenId::Performance, 5);
                }, LV_EVENT_CLICKED);
            }
            recall_op_lbl_ = createLabel(top, "", 6, 35, 322, &lv_font_montserrat_14, palette::kWarning);
            if (recall_op_lbl_) {
                lv_label_set_long_mode(recall_op_lbl_, LV_LABEL_LONG_MODE_CLIP);
                lv_obj_add_flag(recall_op_lbl_, LV_OBJ_FLAG_CLICKABLE);
                attachLvContext<UiNavigationManager*>(recall_op_lbl_, &nav, [](lv_event_t* e) {
                    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                    auto* n = *static_cast<UiNavigationManager**>(lv_event_get_user_data(e));
                    if (n) n->requestNavigateTo(ScreenId::Performance, 5);
                }, LV_EVENT_CLICKED);
            }

            lv_obj_t* lib_btn = createActionButton(top, "TIMBRES >", 334, 3, 88, 46, nullptr, nullptr, false, false);
            attachLvContext<UiNavigationManager*>(lib_btn, &nav, [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                auto* n = *static_cast<UiNavigationManager**>(lv_event_get_user_data(e));
                if (n) n->requestNavigateTo(ScreenId::PatchBrowser);
            }, LV_EVENT_CLICKED);

            struct PillNavCtx { UiNavigationManager* nav; ScreenId screen; bool harmony_overlay; };
            const auto bindPill = [&](lv_obj_t* pill, ScreenId target, bool harmony_overlay = false) {
                if (!pill) return;
                lv_obj_add_flag(pill, LV_OBJ_FLAG_CLICKABLE);
                attachLvContext<PillNavCtx>(pill, {&nav, target, harmony_overlay}, [](lv_event_t* e) {
                    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                    const auto* c = static_cast<const PillNavCtx*>(lv_event_get_user_data(e));
                    if (!c || !c->nav) return;
                    if (c->harmony_overlay && c->nav->requestOpenHarmonyOverlay()) return;
                    c->nav->requestNavigateTo(c->screen);
                }, LV_EVENT_CLICKED);
            };
            arp_pill_  = createPill(top, "ARP OFF",  426, 3, 84, 46, palette::kUnavailableBadge);
            seq_pill_  = createPill(top, "SEQ OFF",  514, 3, 86, 46, palette::kUnavailableBadge);
            fx_pill_   = createPill(top, "FX ON",    604, 3, 82, 46, palette::kActive);
            harm_pill_ = createPill(top, "HARM OFF", 690, 3, 88, 46, palette::kUnavailableBadge);
            bindPill(arp_pill_, ScreenId::Arpeggiator);
            bindPill(seq_pill_, ScreenId::Sequencer);
            bindPill(fx_pill_, ScreenId::Effects);
            bindPill(harm_pill_, ScreenId::Harmony, true);
        }

        // 2. Stage V3 Sub-Mode Bar (y = 61..97)
        struct ModeDef { const char* label; uint8_t tab; bool plan; };
        static constexpr ModeDef kModes[5] = {
            {"MACROS 1-4",     0, false},
            {"MACROS 5-8",     4, false},
            {"CENAS 1-8",      5, false},
            {"TECLADO",        1, false},
            {"ENCADEAR [PLAN]",6, true}
        };
        struct ModeCtx { UiNavigationManager* nav; uint8_t tab; };
        for (uint8_t i = 0; i < 5; ++i) {
            const bool sel = (view_mode == kModes[i].tab);
            lv_obj_t* btn = createActionButton(
                parent, kModes[i].label, 8 + i * 158, 61, 150, 36,
                nullptr, nullptr, sel, sel && !kModes[i].plan);
            attachLvContext<ModeCtx>(btn, {&nav, kModes[i].tab}, [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                auto* c = static_cast<ModeCtx*>(lv_event_get_user_data(e));
                if (c && c->nav) c->nav->requestNavigateTo(ScreenId::Performance, c->tab);
            }, LV_EVENT_CLICKED);
        }

        // 3. Active Stage Body (y = 102..384)
        if (view_mode == 0 || view_mode == 4) {
            buildMacrosStage(parent, backend, nav, (view_mode == 4) ? 4 : 0);
        } else if (view_mode == 5) {
            buildScenesStage(parent, backend, nav);
        } else if (view_mode == 1) {
            buildKeyboardStage(parent, backend, nav);
        } else if (view_mode == 6) {
            buildChainProposalStage(parent);
        } else {
            buildMacrosStage(parent, backend, nav, 0);
        }

        refresh(backend);
        return true;
    }

    void onDestroy() override {
        confirm_dialog_.close(false);
        if (routing_root_) lv_obj_delete(routing_root_);
        routing_root_ = nullptr;
        routing_.reset();
    }

    void refresh(IUiBackend& backend) override {
        backend_ = &backend;
        if (routing_) { routing_->refresh(); return; }
        if (legacy_) {
            refreshLegacy(backend);
            return;
        }

        const uint32_t demo_panics = backend.model().diagnostics().demo_panic_count;
        const char* last_id = backend.lastCommandParameterId();
        const bool is_panic_now =
            (last_id && std::strcmp(last_id, "panic") == 0 &&
             backend.lastCommandState() == UiCommandState::Requested);
        const bool panic_triggered =
            (demo_panics != last_demo_panic_count_) ||
            (is_panic_now && !panic_active_);
        panic_active_ = is_panic_now;
        last_demo_panic_count_ = demo_panics;
        if (panic_triggered && confirm_dialog_.isOpen()) {
            confirm_dialog_.close(false);
        }

        // Context banner updates
        if (patch_lbl_) {
            const auto& patch = backend.model().activePatch();
            char pbuf[96]{};
            std::snprintf(pbuf, sizeof(pbuf), "PATCH #%03u %s (%s)",
                          unsigned(patch.id), patch.name, synthFamilyName(patch.family));
            setLabelIfChanged(patch_lbl_, pbuf);
        }

        const SceneView sv = backend.sceneView();
        if (sv.recall.pending < scene::kSlots) {
            s_last_recall_slot_ = sv.recall.pending;
            s_last_recall_token_ = sv.recall.token;
        } else if (sv.recall.token != 0 && sv.recall.token != s_last_recall_token_) {
            s_last_recall_token_ = sv.recall.token;
            if (!s_last_recall_from_ui_) {
                if (sv.recall.phase == scene::RecallPhase::Confirmed && sv.recall.active < scene::kSlots) {
                    s_last_recall_slot_ = sv.recall.active;
                } else {
                    s_last_recall_slot_ = seq::kNoSlot;
                }
            }
            s_last_recall_from_ui_ = false;
        }
        const uint8_t op_slot = (sv.recall.pending < scene::kSlots)
                                    ? sv.recall.pending
                                    : s_last_recall_slot_;
        const char* active_sc = (sv.recall.active < scene::kSlots && backend.sceneSlot(sv.recall.active))
                                    ? backend.sceneSlot(sv.recall.active)->config.name
                                    : "--";
        const arp::Status* ast = backend.arpStatus();
        const seq::Status* sst = backend.sequencerStatus();
        const uint8_t locks_mask = sessionLockMask();

        if (scene_lbl_) {
            char pat_part[28]{};
            if (sst) {
                const bool has_act = sst->active_slot < seq::kPatternSlots;
                const bool has_pend = sst->pending_slot < seq::kPatternSlots;
                if (has_act && has_pend) {
                    std::snprintf(pat_part, sizeof(pat_part), " | PAT %02u->%02u",
                                  unsigned(sst->active_slot + 1U), unsigned(sst->pending_slot + 1U));
                } else if (has_pend) {
                    std::snprintf(pat_part, sizeof(pat_part), " | PAT ->%02u",
                                  unsigned(sst->pending_slot + 1U));
                } else if (has_act) {
                    std::snprintf(pat_part, sizeof(pat_part), " | PAT %02u%s",
                                  unsigned(sst->active_slot + 1U), sst->modified ? "*" : "");
                } else if (sst->modified) {
                    std::snprintf(pat_part, sizeof(pat_part), " | PAT RAM*");
                }
            }
            char sbuf[96]{};
            std::snprintf(sbuf, sizeof(sbuf), "SC: %s%s%s", active_sc, pat_part,
                          locks_mask ? " | LK" : "");
            setLabelIfChanged(scene_lbl_, sbuf);
        }

        if (recall_op_lbl_) {
            char obuf[64]{};
            uint32_t ocol = palette::kTextSecondary;
            const char* op_res = sceneOpResultShort(sv.recall.phase);
            if (op_res) {
                if (op_slot < scene::kSlots) {
                    std::snprintf(obuf, sizeof(obuf), "RECALL C%u: %s",
                                  unsigned(op_slot + 1U), op_res);
                } else {
                    std::snprintf(obuf, sizeof(obuf), "RECALL: %s", op_res);
                }
                if (sv.recall.phase == scene::RecallPhase::Failed) {
                    ocol = palette::kErrorPanic;
                } else if (sv.recall.phase == scene::RecallPhase::PartialApply ||
                           sv.recall.phase == scene::RecallPhase::Cancelled) {
                    ocol = palette::kWarning;
                } else {
                    ocol = palette::kAccentPrimary;
                }
            } else if (sv.notice && (std::strstr(sv.notice, "REJECTED") != nullptr ||
                                     std::strstr(sv.notice, "PATTERN REFERENCE") != nullptr ||
                                     std::strstr(sv.notice, "BUSY") != nullptr)) {
                if (s_last_recall_slot_ < scene::kSlots) {
                    std::snprintf(obuf, sizeof(obuf), "RECALL C%u: REJECTED",
                                  unsigned(s_last_recall_slot_ + 1U));
                } else {
                    std::snprintf(obuf, sizeof(obuf), "RECALL: REJECTED");
                }
                ocol = palette::kErrorPanic;
            }
            setLabelIfChanged(recall_op_lbl_, obuf);
            lv_obj_set_style_text_color(recall_op_lbl_, lv_color_hex(ocol), 0);
        }

        const bool arp_on = ast ? ast->config.enabled : backend.model().getBool("arp.enable", false);
        const bool seq_on = sst ? sst->playing : backend.model().getBool("seq.playing", false);
        const bool fx_bypass = backend.model().getBool("fx.bypass_all", false);
        const bool q_on = backend.model().getBool("harmony.scale_enable", false);
        const bool ch_on = backend.model().getBool("harmony.chord_enable", false);

        if (arp_pill_) {
            const bool arp_lat = ast && ast->config.latch && !ast->held_count && ast->length > 0;
            const bool arp_run = ast && ast->length > 0;
            const bool arp_arm = ast && ast->config.enabled && ast->armed && !arp_run;
            const char* atxt = !arp_on ? "ARP OFF" :
                               arp_lat ? "ARP LAT" :
                               arp_run ? "ARP RUN" :
                               arp_arm ? "ARP ARM" : "ARP ON";
            const uint32_t abg = !arp_on ? palette::kUnavailableBadge :
                                 arp_arm ? palette::kAccentPrimary : palette::kActive;
            lv_obj_set_style_bg_color(arp_pill_, lv_color_hex(abg), 0);
            if (lv_obj_t* l = lv_obj_get_child(arp_pill_, 0)) {
                setLabelIfChanged(l, atxt);
                lv_obj_set_style_text_color(l, lv_color_hex(arp_on ? palette::kBackground : palette::kTextPrimary), 0);
            }
        }
        if (seq_pill_) {
            const bool pat_pend = sst && sst->pending_slot < seq::kPatternSlots;
            const uint32_t sbg = pat_pend ? palette::kAccentPrimary :
                                 seq_on   ? palette::kActive : palette::kUnavailableBadge;
            lv_obj_set_style_bg_color(seq_pill_, lv_color_hex(sbg), 0);
            if (lv_obj_t* l = lv_obj_get_child(seq_pill_, 0)) {
                char sb[16]{};
                if (pat_pend) {
                    std::snprintf(sb, sizeof(sb), "SEQ P>%u", unsigned(sst->pending_slot + 1U));
                } else if (seq_on && sst && sst->current_step < seq::kSteps) {
                    std::snprintf(sb, sizeof(sb), "SEQ S%u", unsigned(sst->current_step + 1U));
                } else {
                    std::snprintf(sb, sizeof(sb), seq_on ? "SEQ RUN" : "SEQ OFF");
                }
                setLabelIfChanged(l, sb);
                lv_obj_set_style_text_color(l, lv_color_hex((seq_on || pat_pend) ? palette::kBackground : palette::kTextPrimary), 0);
            }
        }
        if (fx_pill_) {
            const bool fx_avail = backend.isDemoMode() ||
                                  (backend.isConnected() &&
                                   backend.parameterStatus("fx.bypass_all") == FeatureStatus::Available);
            const char* fx_txt = !fx_avail ? "FX N/A" : (fx_bypass ? "FX BYP" : "FX ON");
            const uint32_t col = !fx_avail ? palette::kUnavailableBadge :
                                 fx_bypass ? palette::kWarning : palette::kActive;
            const uint32_t ink = fx_avail ? palette::kBackground : palette::kTextPrimary;
            lv_obj_set_style_bg_color(fx_pill_, lv_color_hex(col), 0);
            if (lv_obj_t* l = lv_obj_get_child(fx_pill_, 0)) {
                setLabelIfChanged(l, fx_txt);
                lv_obj_set_style_text_color(l, lv_color_hex(ink), 0);
            }
        }
        if (harm_pill_) {
            const bool harm_avail = backend.isDemoMode() || backend.model().live_harmony.available;
            const bool hon = harm_avail && (q_on || ch_on);
            lv_obj_set_style_bg_color(harm_pill_, lv_color_hex(hon ? palette::kAccentPrimary : palette::kUnavailableBadge), 0);
            if (lv_obj_t* l = lv_obj_get_child(harm_pill_, 0)) {
                const char* htxt = !harm_avail ? "HARM N/A" :
                                   (q_on && ch_on) ? "Q+CH" :
                                   q_on ? "Q ON" :
                                   ch_on ? "CH ON" : "HARM OFF";
                setLabelIfChanged(l, htxt);
                lv_obj_set_style_text_color(l, lv_color_hex(hon ? palette::kBackground : palette::kTextPrimary), 0);
            }
        }

        // Quick 8-Scene strip refresh (Macros 1-4 / 5-8)
        for (unsigned i = 0; i < scene::kSlots; ++i) {
            if (!quick_[i]) continue;
            const auto* slot = backend.sceneSlot(i);
            const bool valid = slot && slot->state == seq::SlotState::Valid;
            const bool is_pend = (sv.recall.pending == i);
            const bool is_act = (sv.recall.active == i);
            const bool is_op = (op_slot < scene::kSlots) && (op_slot == i);
            const bool is_fail = (is_op && (sv.recall.phase == scene::RecallPhase::Failed ||
                                            sv.recall.phase == scene::RecallPhase::PartialApply));

            if (valid) lv_obj_remove_state(quick_[i], LV_STATE_DISABLED);
            else lv_obj_add_state(quick_[i], LV_STATE_DISABLED);

            const uint32_t bg = is_pend ? palette::kWarning :
                                is_fail ? palette::kErrorPanic :
                                is_act  ? palette::kActive :
                                valid   ? palette::kPanelHighlight : palette::kUnavailableBadge;
            lv_obj_set_style_bg_color(quick_[i], lv_color_hex(bg), 0);
            if (lv_obj_t* l = lv_obj_get_child(quick_[i], 0)) {
                char sname[20]{};
                if (valid && slot->config.name[0]) {
                    std::snprintf(sname, sizeof(sname), "C%u %.5s", i + 1U, slot->config.name);
                } else {
                    std::snprintf(sname, sizeof(sname), "C%u ---", i + 1U);
                }
                setLabelIfChanged(l, sname);
                const uint32_t ink = (is_pend || is_act) ? palette::kBackground : palette::kTextPrimary;
                lv_obj_set_style_text_color(l, lv_color_hex(ink), 0);
            }
        }

        // Full Cenas 1-8 tab refresh
        for (unsigned s = 0; s < scene::kSlots; ++s) {
            if (!scene_cards_[s].btn) continue;
            const scene::Slot* slot = backend.sceneSlot(s);
            const bool sel = (selected_scene_slot_ == s);
            const bool is_act = (sv.recall.active == s);
            const bool is_pend = (sv.recall.pending == s);
            const bool is_op = (op_slot < scene::kSlots) && (op_slot == s);
            const char* ph = is_pend ? "PENDING" :
                             (is_act ? "ACTIVE" : scenePhaseShort(scene::RecallPhase::Idle, slot));
            if (is_op && sv.recall.phase == scene::RecallPhase::PartialApply) ph = "PARTIAL";
            else if (is_op && sv.recall.phase == scene::RecallPhase::WaitingPatch) ph = "WAIT PATCH";
            else if (is_op && sv.recall.phase == scene::RecallPhase::Failed) ph = "FAILED";
            else if (is_op && sv.recall.phase == scene::RecallPhase::Cancelled) ph = "CANCELLED";

            if (scene_cards_[s].badge) {
                lv_obj_set_style_bg_color(scene_cards_[s].badge, lv_color_hex(sceneStateColor(ph)), 0);
                if (lv_obj_t* l = lv_obj_get_child(scene_cards_[s].badge, 0)) {
                    setLabelIfChanged(l, ph);
                    lv_obj_set_style_text_color(l, lv_color_hex(sceneBadgeTextColor(ph)), 0);
                }
            }
            if (scene_cards_[s].card) {
                lv_obj_set_style_border_width(scene_cards_[s].card, sel ? 2 : 1, 0);
                lv_obj_set_style_border_color(
                    scene_cards_[s].card,
                    lv_color_hex(sel ? palette::kAccentPrimary : palette::kBorder), 0);
            }
            if (lv_obj_t* l = lv_obj_get_child(scene_cards_[s].btn, 0)) {
                char lbl[32]{};
                if (slot && slot->state == seq::SlotState::Valid) {
                    std::snprintf(lbl, sizeof(lbl), "C%u: %.10s", s + 1U, slot->config.name);
                } else if (slot && slot->state == seq::SlotState::Corrupted) {
                    std::snprintf(lbl, sizeof(lbl), "C%u: (CORRUPT)", s + 1U);
                } else if (slot && slot->state == seq::SlotState::Incompatible) {
                    std::snprintf(lbl, sizeof(lbl), "C%u: (INCOMP)", s + 1U);
                } else {
                    std::snprintf(lbl, sizeof(lbl), "C%u: (EMPTY)", s + 1U);
                }
                setLabelIfChanged(l, lbl);
            }
        }
        if (scenes_notice_lbl_) {
            char nbuf[112]{};
            const char* notice_txt = (sv.notice && sv.notice[0]) ? sv.notice : scenePhaseShort(sv.recall.phase, nullptr);
            std::snprintf(nbuf, sizeof(nbuf), "SEL: C%u | ATIVA: %s | %s",
                          unsigned(selected_scene_slot_ + 1U), active_sc, notice_txt);
            setLabelIfChanged(scenes_notice_lbl_, nbuf);
        }
        if (scenes_domains_lbl_) {
            const scene::Slot* sel_slot = backend.sceneSlot(selected_scene_slot_);
            char dbuf[144]{"RECALL TOTAL: -- (SLOT VAZIO)"};
            if (sel_slot && sel_slot->state == seq::SlotState::Valid) {
                const uint8_t stored = sel_slot->config.domains;
                SceneOperation op{};
                op.action = SceneAction::Load;
                op.slot = selected_scene_slot_;
                op.domains = stored;
                op.locks = sessionLockMask();
                op.mode = scene::RecallMode::Immediate;
                op.apply_tempo = false; // Matches RECALL button's SceneOperation default
                const uint8_t eff = scene::effectiveDomains(stored, op.domains, op.locks, op.apply_tempo);
                const scene::Reject compat = backend.sceneCompatibility(op);
                static constexpr const char* kDomNames[7] = {
                    "PATCH*", "PATTERN", "FX", "HARMONY", "ARP", "MACROS", "TEMPO"
                };
                static constexpr uint8_t kDomLocks[7] = {
                    0, 0, perf_lock::Fx, perf_lock::Harmony, perf_lock::Arp, perf_lock::Macros, 0
                };
                unsigned used = static_cast<unsigned>(std::snprintf(dbuf, sizeof(dbuf), "RECALL TOTAL: "));
                unsigned count = 0;
                for (unsigned d = 0; d < 7 && used + 16 < sizeof(dbuf); ++d) {
                    const uint8_t bit = static_cast<uint8_t>(1U << d);
                    if ((eff & bit) != 0U) {
                        used += static_cast<unsigned>(std::snprintf(
                            dbuf + used, sizeof(dbuf) - used, "%s%s", count ? " | " : "", kDomNames[d]));
                        ++count;
                    } else if ((stored & bit) != 0U && (op.locks & kDomLocks[d]) != 0U) {
                        used += static_cast<unsigned>(std::snprintf(
                            dbuf + used, sizeof(dbuf) - used, "%s%s(LK)", count ? " | " : "", kDomNames[d]));
                        ++count;
                    } else if ((stored & bit) != 0U && bit == scene::Tempo && !op.apply_tempo) {
                        used += static_cast<unsigned>(std::snprintf(
                            dbuf + used, sizeof(dbuf) - used, "%sTEMPO(OFF)", count ? " | " : ""));
                        ++count;
                    }
                }
                if (eff == 0) {
                    std::snprintf(dbuf, sizeof(dbuf), "RECALL TOTAL: NENHUM%s",
                                  (stored == scene::Tempo && !op.apply_tempo) ? " (TEMPO OFF)" : " (LOCKED)");
                    used = static_cast<unsigned>(std::strlen(dbuf));
                }
                if (compat != scene::Reject::None && used + 16 < sizeof(dbuf)) {
                    const char* ctxt =
                        (compat == scene::Reject::PatternUnavailable) ? " [PAT N/A]" :
                        (compat == scene::Reject::PatchUnavailable)   ? " [PATCH N/A]" :
                        (compat == scene::Reject::IncompatibleMacros) ? " [MAC INCOMP]" :
                        (compat == scene::Reject::CompareActive)      ? " [COMPARE]" :
                        (compat == scene::Reject::EmptySelection)     ? " [NO DOM]" : " [INCOMPAT]";
                    std::snprintf(dbuf + used, sizeof(dbuf) - used, "%s", ctxt);
                }
            } else if (sel_slot && sel_slot->state == seq::SlotState::Corrupted) {
                std::snprintf(dbuf, sizeof(dbuf), "RECALL TOTAL: -- (CORRUPT)");
            } else if (sel_slot && sel_slot->state == seq::SlotState::Incompatible) {
                std::snprintf(dbuf, sizeof(dbuf), "RECALL TOTAL: -- (INCOMP)");
            }
            setLabelIfChanged(scenes_domains_lbl_, dbuf);
        }
        const uint8_t locks = sessionLockMask();
        static constexpr uint8_t kLockBits[4] = {perf_lock::Fx, perf_lock::Harmony, perf_lock::Arp, perf_lock::Macros};
        for (unsigned li = 0; li < 4; ++li) {
            if (!lock_btns_[li]) continue;
            const bool on = (locks & kLockBits[li]) != 0U;
            if (on) lv_obj_add_state(lock_btns_[li], LV_STATE_CHECKED);
            else lv_obj_remove_state(lock_btns_[li], LV_STATE_CHECKED);
        }

        if (oct_lbl_) {
            const int32_t oct = backend.model().getInt("perf.octave_shift", 0);
            char obuf[16]{};
            std::snprintf(obuf, sizeof(obuf), "OCT %+ld", static_cast<long>(oct));
            setLabelIfChanged(oct_lbl_, obuf);
        }
        if (kb_) {
            refreshVirtualKeyboard(kb_);
        }
    }

private:
    struct QuickScene { IUiBackend* backend; uint8_t slot; };
    struct SceneSlotCtx { PerformanceScreen* screen; IUiBackend* backend; uint8_t slot; int action; };
    struct SceneCard { lv_obj_t* card = nullptr; lv_obj_t* btn = nullptr; lv_obj_t* badge = nullptr; };

    void buildMacrosStage(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav, uint8_t base_macro) {
        static constexpr const char* kMacroIds[8] = {
            "perf.macro.1", "perf.macro.2", "perf.macro.3", "perf.macro.4",
            "perf.macro.5", "perf.macro.6", "perf.macro.7", "perf.macro.8"
        };
        static constexpr const char* kMacroLabels[8] = {
            "CHARACTER", "BRIGHTNESS", "MOTION", "SHAPE", "ATTACK", "RELEASE", "SPACE", "EDGE"
        };
        for (uint8_t i = 0; i < 4; ++i) {
            const uint8_t m_idx = static_cast<uint8_t>(base_macro + i);
            const int32_t x = 8 + (i % 2) * 394;
            const int32_t y = 102 + (i / 2) * 86;
            createParameterSlider(parent, backend, kMacroIds[m_idx], x, y, 390, 80,
                                  PresentationLevel::StageMinimal, kMacroLabels[m_idx]);
        }

        // Quick 8-Scene Recall Strip (y = 278..384)
        lv_obj_t* qs = lv_obj_create(parent);
        if (!qs) return;
        applyPanelStyle(qs, false);
        lv_obj_set_pos(qs, 8, 278);
        lv_obj_set_size(qs, 784, 102);
        lv_obj_set_style_pad_all(qs, 4, 0);
        lv_obj_remove_flag(qs, LV_OBJ_FLAG_SCROLLABLE);

        createLabel(qs, "ACESSO RAPIDO - CENAS 1..8 (TOQUE P/ RECALL)", 6, 4, 520,
                    &lv_font_montserrat_14, palette::kTextSecondary);

        lv_obj_t* sel_btn = createActionButton(qs, "SELETIVO >", 654, 2, 120, 32, nullptr, nullptr, false, true);
        attachLvContext<UiNavigationManager*>(sel_btn, &nav, [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
            auto* n = *static_cast<UiNavigationManager**>(lv_event_get_user_data(e));
            if (n) n->requestNavigateTo(ScreenId::Scenes);
        }, LV_EVENT_CLICKED);

        const bool has_scenes = (backend.sceneSlot(0) != nullptr);
        for (unsigned s = 0; s < scene::kSlots; ++s) {
            char num[12]{};
            std::snprintf(num, sizeof(num), "C%u ---", s + 1U);
            quick_[s] = createActionButton(qs, num, 6 + int32_t(s) * 96, 40, 92, 46, nullptr, nullptr);
            if (quick_[s] && has_scenes) {
                attachLvContext<QuickScene>(quick_[s], {&backend, uint8_t(s)}, [](lv_event_t* e) {
                    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                    const auto& q = *static_cast<const QuickScene*>(lv_event_get_user_data(e));
                    s_last_recall_slot_ = q.slot;
                    s_last_recall_from_ui_ = true;
                    SceneOperation op;
                    op.slot = q.slot;
                    op.locks = sessionLockMask();
                    q.backend->requestScene(op);
                }, LV_EVENT_CLICKED);
            }
        }
    }

    void buildScenesStage(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav) {
        if (!backend.sceneSlot(0)) {
            createLabel(parent, "SCENE LIBRARY UNAVAILABLE (DEMO)", 16, 120, 760,
                        &lv_font_montserrat_14, palette::kWarning);
            return;
        }
        for (uint8_t s = 0; s < scene::kSlots; ++s) {
            const int32_t x = 8 + (s % 4) * 198;
            const int32_t y = 102 + (s / 4) * 86;
            lv_obj_t* card = lv_obj_create(parent);
            applyPanelStyle(card, false);
            lv_obj_set_pos(card, x, y);
            lv_obj_set_size(card, 190, 80);
            lv_obj_set_style_pad_all(card, 4, 0);
            lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t* badge = createPill(card, "EMPTY", 92, 2, 88, 22, palette::kPlannedBadge);
            lv_obj_t* btn = createActionButton(card, "C1", 2, 28, 178, 42, nullptr, nullptr, false, false);
            scene_cards_[s] = {card, btn, badge};

            attachLvContext<SceneSlotCtx>(btn, {this, &backend, s, 0}, [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                auto* c = static_cast<SceneSlotCtx*>(lv_event_get_user_data(e));
                if (!c || !c->screen || !c->backend) return;
                c->screen->selected_scene_slot_ = c->slot;
                c->screen->refresh(*c->backend);
            }, LV_EVENT_CLICKED);
        }

        lv_obj_t* bar = lv_obj_create(parent);
        applyPanelStyle(bar, false);
        lv_obj_set_pos(bar, 8, 278);
        lv_obj_set_size(bar, 784, 102);
        lv_obj_set_style_pad_all(bar, 4, 0);
        lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

        scenes_notice_lbl_ = createLabel(bar, "CENA 1 | --", 6, 2, 768, &lv_font_montserrat_14, palette::kTextSecondary);
        scenes_domains_lbl_ = createLabel(bar, "RECALL TOTAL: --", 6, 20, 768, &lv_font_montserrat_14, palette::kAccentPrimary);

        const auto add_act = [&](const char* lbl, int32_t x, int32_t w, int act, bool acc = false) {
            lv_obj_t* b = createActionButton(bar, lbl, x, 40, w, 46, nullptr, nullptr, acc, acc);
            attachLvContext<SceneSlotCtx>(b, {this, &backend, 0, act}, [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                auto* c = static_cast<SceneSlotCtx*>(lv_event_get_user_data(e));
                if (!c || !c->screen || !c->backend) return;
                if (c->action == 1) {
                    s_last_recall_slot_ = c->screen->selected_scene_slot_;
                    s_last_recall_from_ui_ = true;
                    SceneOperation op;
                    op.action = SceneAction::Load;
                    op.slot = c->screen->selected_scene_slot_;
                    op.locks = sessionLockMask();
                    c->backend->requestScene(op);
                    c->screen->refresh(*c->backend);
                } else if (c->action == 2) {
                    c->screen->openSceneConfirm(SceneAction::Save);
                } else if (c->action == 3) {
                    c->screen->openSceneConfirm(SceneAction::Delete);
                }
            }, LV_EVENT_CLICKED);
        };
        add_act("RECALL", 6, 88, 1, true);

        lv_obj_t* mod_btn = createActionButton(bar, "SELETIVO...", 98, 40, 114, 46, nullptr, nullptr, false, false);
        attachLvContext<UiNavigationManager*>(mod_btn, &nav, [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
            auto* n = *static_cast<UiNavigationManager**>(lv_event_get_user_data(e));
            if (n) n->requestNavigateTo(ScreenId::Scenes);
        }, LV_EVENT_CLICKED);

        add_act("SALVAR", 216, 86, 2);
        add_act("LIMPAR", 306, 86, 3);

        static constexpr const char* kLockNames[4] = {"LK FX", "LK HARM", "LK ARP", "LK MAC"};
        static constexpr uint8_t kLockBits[4] = {perf_lock::Fx, perf_lock::Harmony, perf_lock::Arp, perf_lock::Macros};
        for (uint8_t li = 0; li < 4; ++li) {
            lock_btns_[li] = createActionButton(bar, kLockNames[li], 406 + li * 93, 40, 88, 46, nullptr, nullptr, false, false);
            attachLvContext<SceneSlotCtx>(lock_btns_[li], {this, &backend, kLockBits[li], 4}, [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                auto* c = static_cast<SceneSlotCtx*>(lv_event_get_user_data(e));
                if (!c || !c->screen || !c->backend) return;
                setSessionLockMask(sessionLockMask() ^ c->slot);
                c->screen->refresh(*c->backend);
            }, LV_EVENT_CLICKED);
        }
    }

    void buildKeyboardStage(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav) {
        struct OctCtx { IUiBackend* backend; UiNavigationManager* nav; int delta; };
        createParameterSlider(parent, backend, "perf.master_volume", 8, 102, 310, 56,
                              PresentationLevel::StageMinimal, "MASTER LEVEL");

        lv_obj_t* oct_dn = createActionButton(parent, "OCT -", 324, 106, 92, 48, nullptr, nullptr);
        oct_lbl_ = createLabel(parent, "OCT +0", 420, 120, 76, &lv_font_montserrat_14, palette::kAccentPrimary);
        if (oct_lbl_) lv_obj_set_style_text_align(oct_lbl_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_t* oct_up = createActionButton(parent, "OCT +", 500, 106, 92, 48, nullptr, nullptr);

        const auto on_oct = [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
            auto* c = static_cast<OctCtx*>(lv_event_get_user_data(e));
            if (!c || !c->backend || !c->nav) return;
            const int32_t oct = c->backend->model().getInt("perf.octave_shift", 0);
            c->backend->requestParameterInt("perf.octave_shift", oct + c->delta);
            c->nav->refreshActive();
        };
        attachLvContext<OctCtx>(oct_dn, {&backend, &nav, -1}, on_oct, LV_EVENT_CLICKED);
        attachLvContext<OctCtx>(oct_up, {&backend, &nav, 1}, on_oct, LV_EVENT_CLICKED);

        createLabel(parent, "SAIR: SOLTA NOTAS", 600, 120, 192, &lv_font_montserrat_14, palette::kTextSecondary);

        kb_ = createVirtualKeyboard(parent, backend, nav, KeyboardDisplayMode::StageKeys2Oct, 8, 164, 784, 218);
    }

    void buildChainProposalStage(lv_obj_t* parent) {
        lv_obj_t* card = lv_obj_create(parent);
        if (!card) return;
        applyPanelStyle(card, true);
        lv_obj_set_pos(card, 8, 102);
        lv_obj_set_size(card, 784, 276);
        lv_obj_set_style_border_color(card, lv_color_hex(palette::kWarning), 0);
        lv_obj_set_style_pad_all(card, 8, 0);
        lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        createLabel(card, "ENCADEAMENTO DE CENAS - PROPOSTA VISUAL", 6, 4, 540,
                    &lv_font_montserrat_14, palette::kAccentPrimary);
        createPill(card, "NAO IMPLEMENTADO", 584, 2, 180, 26, palette::kWarning, palette::kBackground);
        createLabel(card,
                    "MOCKUP VISUAL SOMENTE - NAO IMPLEMENTADO NO AUDIO OWNER / NVS.",
                    6, 36, 756, &lv_font_montserrat_14, palette::kWarning);

        static constexpr const char* kSteps[4] = {
            "1: CENA 1 [4 COMP.]", "2: CENA 3 [8 COMP.]", "3: CENA 2 [2 COMP.]", "4: LOOP / STOP"
        };
        for (int i = 0; i < 4; ++i) {
            lv_obj_t* box = lv_obj_create(card);
            applyPanelStyle(box, i == 0);
            lv_obj_set_pos(box, 6 + i * 192, 72);
            lv_obj_set_size(box, 184, 96);
            lv_obj_set_style_pad_all(box, 6, 0);
            lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
            createLabel(box, kSteps[i], 4, 6, 168, &lv_font_montserrat_14, palette::kTextPrimary);
            createLabel(box, i == 0 ? "ATUAL (MOCK)" : "FILA (MOCK)", 4, 34, 168, &lv_font_montserrat_14, palette::kTextSecondary);
            createLabel(box, "NEXT BAR", 4, 62, 168, &lv_font_montserrat_14, palette::kPlannedBadge);
        }

        createLabel(card,
                    "Proposta futura: ChainEngine no Audio Owner, troca em fronteira de compasso e desarme manual sem bloquear Panic.",
                    6, 190, 756, &lv_font_montserrat_14, palette::kTextSecondary);
    }

    bool buildLegacy(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav) {
        confirming_ = false;
        createLabel(parent, "LEGACY MACRO OFFSETS", 12, 14, 560, &lv_font_montserrat_14, palette::kAccentPrimary);
        legacy_ = createLabel(parent, "", 12, 70, 760, &lv_font_montserrat_14, palette::kTextSecondary);
        for (unsigned i = 0; i < 3; ++i)
            legacy_rows_[i] = createLabel(parent, "", 12, 112 + 36 * int32_t(i), 760,
                &lv_font_montserrat_14, palette::kTextPrimary);
        confirm_ = createLabel(parent, "", 12, 252, 760, &lv_font_montserrat_14, palette::kAccentPrimary);
        auto* back = createActionButton(parent, "BACK / CANCEL", 12, 302, 184, 44, nullptr, nullptr);
        attachLvContext<UiNavigationManager*>(back, &nav, [](lv_event_t* e) {
            auto* navigation = *static_cast<UiNavigationManager**>(lv_event_get_user_data(e));
            navigation->navigateTo(ScreenId::Performance, 0);
        }, LV_EVENT_CLICKED);
        struct ResetContext { PerformanceScreen* screen; IUiBackend* backend; };
        auto* reset = createActionButton(parent, "RESET LEGACY OFFSETS", 206, 302, 244, 44, nullptr, nullptr);
        attachLvContext<ResetContext>(reset, {this, &backend}, [](lv_event_t* e) {
            auto& context = *static_cast<ResetContext*>(lv_event_get_user_data(e));
            auto& screen = *context.screen;
            if (screen.confirming_) {
                const auto result = context.backend->resetLegacyOffsets(screen.expected_);
                lv_label_set_text(screen.confirm_, result.state == UiCommandState::Requested ?
                    "" : "RESET REJECTED.");
                screen.confirming_ = false;
            } else if (context.backend->legacyOffsets(screen.expected_) && !screen.expected_.compare) {
                screen.confirming_ = true;
                lv_label_set_text(screen.confirm_, "TAP RESET AGAIN TO SET ALL THREE OFFSETS TO 64.");
            }
        }, LV_EVENT_CLICKED);
        status_lbl_ = createLabel(parent, "", 462, 314, 300, &lv_font_montserrat_14, palette::kAccentPrimary);
        refresh(backend);
        return true;
    }

    void refreshLegacy(IUiBackend& backend) {
        IUiBackend::LegacyOffsets offsets;
        const bool available = backend.legacyOffsets(offsets);
        lv_label_set_text(legacy_, !available ? "LEGACY OFFSETS UNAVAILABLE" :
            offsets.fm ? (offsets.compare ? "DX7 | COMPARE ACTIVE" : "DX7") :
                         (offsets.compare ? "JUNO | COMPARE ACTIVE" : "JUNO"));
        constexpr const char* names[]{"BRIGHTNESS", "ATTACK", "RELEASE"};
        constexpr const char* juno[]{"Filter cutoff", "Amp envelope attack", "Amp envelope release"};
        constexpr const char* fm[]{"FM depth", "Juno only", "Juno only"};
        for (unsigned i = 0; i < 3; ++i) {
            char text[96]{};
            if (available) {
                const char* state = offsets.positions[i] == 64 ? "NEUTRAL" :
                    offsets.compare ? "BYPASSED" : "ACTIVE";
                std::snprintf(text, sizeof(text), "%s   %u / 64   %s   %s", names[i],
                    unsigned(offsets.positions[i]), state, offsets.fm ? fm[i] : juno[i]);
            }
            lv_label_set_text(legacy_rows_[i], text);
        }
        if (status_lbl_) {
            char st_buf[32]{};
            std::snprintf(st_buf, sizeof(st_buf), "CMD: %s", commandStateLabel(backend.lastCommandState()));
            lv_label_set_text(status_lbl_, st_buf);
        }
    }

    void openSceneConfirm(SceneAction action) {
        if (!parent_ || !backend_) return;
        const uint8_t slot_idx = selected_scene_slot_;
        const scene::Slot* slot = backend_->sceneSlot(slot_idx);
        if (!slot) return;

        pending_scene_action_ = action;
        pending_scene_slot_ = slot_idx;

        char title[48]{};
        char body[160]{};
        const char* detail = nullptr;
        const char* confirm_lbl = "CONFIRMAR";
        uint32_t border = palette::kAccentPrimary;
        bool destructive = false;

        if (action == SceneAction::Save) {
            if (slot->state == seq::SlotState::Valid) {
                std::snprintf(title, sizeof(title), "CONFIRMAR SOBRESCRITA");
                std::snprintf(body, sizeof(body), "Sobrescrever CENA %u: %s?",
                              unsigned(slot_idx + 1U),
                              slot->config.name[0] ? slot->config.name : "SEM NOME");
                detail = "A cena existente sera substituida pelos dominios capturados atuais.";
                confirm_lbl = "SOBRESCREVER";
                border = palette::kWarning;
            } else if (slot->state == seq::SlotState::Empty) {
                std::snprintf(title, sizeof(title), "CONFIRMAR SALVAMENTO");
                std::snprintf(body, sizeof(body), "Salvar estado atual na CENA %u (SLOT VAZIO)?",
                              unsigned(slot_idx + 1U));
                detail = "Dominios disponiveis serao capturados e gravados no armazenamento.";
                confirm_lbl = "SALVAR CENA";
                border = palette::kAccentPrimary;
            } else {
                const char* st_txt = (slot->state == seq::SlotState::Corrupted) ? "CORRUPT" : "INCOMP";
                std::snprintf(title, sizeof(title), "CONFIRMAR SOBRESCRITA");
                std::snprintf(body, sizeof(body), "Sobrescrever CENA %u (%s)?",
                              unsigned(slot_idx + 1U), st_txt);
                detail = "Slot atual nao contem cena valida. Confirmar gravacao?";
                confirm_lbl = "SOBRESCREVER";
                border = palette::kWarning;
            }
        } else if (action == SceneAction::Delete) {
            std::snprintf(title, sizeof(title), "CONFIRMAR EXCLUSAO");
            if (slot->state == seq::SlotState::Valid) {
                std::snprintf(body, sizeof(body), "Apagar CENA %u: %s?",
                              unsigned(slot_idx + 1U),
                              slot->config.name[0] ? slot->config.name : "SEM NOME");
            } else {
                std::snprintf(body, sizeof(body), "Apagar CENA %u (%s)?",
                              unsigned(slot_idx + 1U),
                              scenePhaseShort(scene::RecallPhase::Idle, slot));
            }
            detail = "Esta acao remove permanentemente a cena do armazenamento.";
            confirm_lbl = "APAGAR CENA";
            border = palette::kErrorPanic;
            destructive = true;
        } else {
            return;
        }

        ConfirmDialogOptions opts{};
        opts.title = title;
        opts.body = body;
        opts.detail = detail;
        opts.cancel_label = "CANCELAR";
        opts.confirm_label = confirm_lbl;
        opts.border_color = border;
        opts.destructive = destructive;
        opts.user_data = this;
        opts.on_confirm = [](void* ud) {
            auto* self = static_cast<PerformanceScreen*>(ud);
            if (self) self->executeConfirmedSceneAction();
        };
        opts.on_cancel = [](void* ud) {
            auto* self = static_cast<PerformanceScreen*>(ud);
            if (self && self->backend_) self->refresh(*self->backend_);
        };
        confirm_dialog_.open(parent_, opts);
    }

    void executeConfirmedSceneAction() {
        if (!backend_) return;
        SceneOperation op;
        op.action = pending_scene_action_;
        op.slot = pending_scene_slot_;
        op.locks = sessionLockMask();
        if (pending_scene_action_ == SceneAction::Save) {
            op.domains = backend_->sceneCaptureDomains();
        }
        backend_->requestScene(op);
        refresh(*backend_);
    }

    lv_obj_t* parent_ = nullptr;
    IUiBackend* backend_ = nullptr;
    ConfirmDialog confirm_dialog_{};
    SceneAction pending_scene_action_ = SceneAction::Load;
    uint8_t pending_scene_slot_ = 0;
    uint32_t last_demo_panic_count_ = 0;
    bool panic_active_ = false;
    std::unique_ptr<RoutingEditor> routing_;
    lv_obj_t* routing_root_ = nullptr;
    IUiBackend::LegacyOffsets expected_{};
    bool confirming_ = false;
    uint8_t selected_scene_slot_ = 0;
    static inline uint8_t s_last_recall_slot_ = seq::kNoSlot;
    static inline uint32_t s_last_recall_token_ = 0;
    static inline bool s_last_recall_from_ui_ = false;
    lv_obj_t* legacy_ = nullptr;
    lv_obj_t* legacy_rows_[3]{};
    lv_obj_t* confirm_ = nullptr;
    lv_obj_t* oct_lbl_    = nullptr;
    lv_obj_t* status_lbl_ = nullptr;
    lv_obj_t* kb_         = nullptr;
    lv_obj_t* patch_lbl_  = nullptr;
    lv_obj_t* scene_lbl_  = nullptr;
    lv_obj_t* recall_op_lbl_ = nullptr;
    lv_obj_t* arp_pill_   = nullptr;
    lv_obj_t* seq_pill_   = nullptr;
    lv_obj_t* fx_pill_    = nullptr;
    lv_obj_t* harm_pill_  = nullptr;
    lv_obj_t* quick_[scene::kSlots]{};
    SceneCard scene_cards_[scene::kSlots]{};
    lv_obj_t* lock_btns_[4]{};
    lv_obj_t* scenes_notice_lbl_ = nullptr;
    lv_obj_t* scenes_domains_lbl_ = nullptr;
};

} // namespace

ScreenView* createPerformanceScreen() {
    return new PerformanceScreen();
}

} // namespace smk::p4::ui
