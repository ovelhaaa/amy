#include "ui_widgets.h"
#include "../../amy_runtime/performance_lock.h"
#include <cstdio>

namespace smk::p4::ui {

namespace {
static uint8_t s_session_lock_mask = perf_lock::kDefaultLockMask;

struct PatchSelCtx {
    IUiBackend*          backend = nullptr;
    UiNavigationManager* nav     = nullptr;
    int                  action  = 0; // -1=Prev, +1=Next, 2=Browse, 3=Fav, 4=SoundEdit
};

struct DialogState {
    IUiBackend* backend = nullptr;
    lv_obj_t* root = nullptr;
    lv_obj_t* status = nullptr;
    lv_obj_t* clean_btn = nullptr;
    lv_obj_t* locked_btn = nullptr;
    uint16_t target_patch = 0;
    uint16_t original_patch = 0;
    uint8_t layer = 0;
    bool dirty = false;
    bool in_flight = false;
    uint32_t request_start_ms = 0;
};

static void executeLoad(DialogState* s, bool with_locks) {
    if (!s || s->in_flight) return;
    if (s->original_patch != s->backend->model().activePatch().id) {
        lv_label_set_text(s->status, "Current patch changed. Cancel and select again.");
        lv_obj_set_style_text_color(s->status, lv_color_hex(palette::kWarning), 0);
        return;
    }
    const uint8_t mask = with_locks ? s_session_lock_mask : static_cast<uint8_t>(perf_lock::None);
    UiCommandResult result{};
    if (s->dirty) {
        result = s->backend->confirmPatchDiscard(s->target_patch, s->layer, mask);
    } else {
        result = s->backend->requestSelectPatch(s->target_patch, s->layer, mask);
    }
    if (!result.accepted()) {
        lv_label_set_text(s->status, result.notice ? result.notice : "Patch change unavailable.");
        lv_obj_set_style_text_color(s->status, lv_color_hex(palette::kWarning), 0);
        return;
    }
    if (result.state == UiCommandState::Confirmed) {
        lv_obj_delete(s->root);
        return;
    }
    s->in_flight = true;
    s->request_start_ms = lv_tick_get();
    lv_label_set_text(s->status, "LOADING PATCH...");
    lv_obj_set_style_text_color(s->status, lv_color_hex(palette::kAccentPrimary), 0);
    lv_obj_add_state(s->clean_btn, LV_STATE_DISABLED);
    lv_obj_add_state(s->locked_btn, LV_STATE_DISABLED);
}

struct BtnContext { DialogState* state; bool with_locks; };

} // namespace

uint8_t sessionLockMask() { return s_session_lock_mask; }
void setSessionLockMask(uint8_t mask) { s_session_lock_mask = mask; }

void selectPatchWithConfirmation(IUiBackend& backend, UiNavigationManager& nav, uint16_t patch, uint8_t layer, bool force_dialog) {
    if (backend.isBusy()) return;
    if (layer) {
        backend.requestSelectPatch(patch, layer);
        return;
    }
    const bool dirty = backend.patchNeedsDiscardConfirmation();
    if (!force_dialog && !dirty) {
        backend.requestSelectPatch(patch, layer, s_session_lock_mask);
        return;
    }
    static lv_obj_t* confirmation = nullptr;
    if (confirmation) return;
    auto* root = lv_obj_create(nav.contentRoot());
    if (!root) return;
    confirmation = root;
    lv_obj_add_event_cb(root, [](lv_event_t*) { confirmation = nullptr; }, LV_EVENT_DELETE, nullptr);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_70, 0);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    auto* panel = lv_obj_create(root);
    applyPanelStyle(panel, true);
    lv_obj_set_size(panel, 620, 276);
    lv_obj_center(panel);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    size_t count = 0;
    const auto* cat = UiModel::catalog(count);
    const char* patch_name = "";
    for (size_t i = 0; i < count; ++i) {
        if (cat[i].id == patch) { patch_name = cat[i].name; break; }
    }

    char heading[80];
    if (patch_name[0]) {
        std::snprintf(heading, sizeof(heading), "LOAD PATCH %03u \xE2\x80\x94 %s?", unsigned(patch), patch_name);
    } else {
        std::snprintf(heading, sizeof(heading), "LOAD PATCH %03u?", unsigned(patch));
    }
    createLabel(panel, heading, 16, 12, 588, &lv_font_montserrat_20, palette::kTextPrimary);

    createLabel(panel,
        dirty ? "Unsaved changes to the current patch will be lost."
              : "Selective Performance Lock: choose settings to preserve.",
        16, 42, 588, &lv_font_montserrat_14, dirty ? palette::kWarning : palette::kAccentPrimary);

    auto make_cb = [&](const char* text, int x, int y, uint8_t flag) {
        auto* cb = lv_checkbox_create(panel);
        lv_checkbox_set_text(cb, text);
        lv_obj_set_pos(cb, x, y);
        lv_obj_set_size(cb, 280, 36);
        constexpr auto checked_st = lv_style_selector_t(LV_PART_INDICATOR) | lv_style_selector_t(LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(cb, lv_color_hex(palette::kAccentPrimary), checked_st);
        if (s_session_lock_mask & flag) lv_obj_add_state(cb, LV_STATE_CHECKED);
        else lv_obj_clear_state(cb, LV_STATE_CHECKED);
        struct CbCtx { uint8_t flag; };
        attachLvContext<CbCtx>(cb, {flag}, [](lv_event_t* e) {
            auto* c = static_cast<CbCtx*>(lv_event_get_user_data(e));
            auto* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
            if (lv_obj_has_state(target, LV_STATE_CHECKED)) s_session_lock_mask |= c->flag;
            else s_session_lock_mask &= ~c->flag;
        }, LV_EVENT_VALUE_CHANGED);
        return cb;
    };

    make_cb("KEEP FX (CHORUS/DLY/REV/EQ)", 16, 78, perf_lock::Fx);
    make_cb("KEEP HARMONY / CHORD", 310, 78, perf_lock::Harmony);
    make_cb("KEEP ARPEGGIATOR (RATES/CFG)", 16, 122, perf_lock::Arp);
    make_cb("KEEP MACROS (ADV)", 310, 122, perf_lock::Macros);

    auto* status = createLabel(panel, "", 16, 168, 588, &lv_font_montserrat_14, palette::kWarning);

    auto* cancel = createActionButton(panel, "CANCEL", 16, 206, 100, 44, nullptr, nullptr);
    auto* clean_btn = createActionButton(panel, dirty ? "DISCARD & LOAD" : "LOAD PATCH",
                                         128, 206, dirty ? 174 : 164, 44, nullptr, nullptr);
    auto* locked_btn = createActionButton(panel, dirty ? "DISCARD & LOAD WITH LOCKS" : "LOAD WITH LOCKS",
                                          dirty ? 314 : 304, 206, dirty ? 290 : 300, 44, nullptr, nullptr, false, true);

    lv_obj_add_event_cb(cancel, [](lv_event_t* event) {
        lv_obj_delete(static_cast<lv_obj_t*>(lv_event_get_user_data(event)));
    }, LV_EVENT_CLICKED, root);

    auto* state = new DialogState{&backend, root, status, clean_btn, locked_btn, patch, backend.model().activePatch().id, layer, dirty, false, 0};
    lv_obj_add_event_cb(root, [](lv_event_t* e) {
        auto* s = static_cast<DialogState*>(lv_event_get_user_data(e));
        delete s;
    }, LV_EVENT_DELETE, state);

    attachLvContext<BtnContext>(clean_btn, {state, false}, [](lv_event_t* e) {
        auto* ctx = static_cast<BtnContext*>(lv_event_get_user_data(e));
        ctx->state->clean_btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
        executeLoad(ctx->state, ctx->with_locks);
    }, LV_EVENT_CLICKED);
    attachLvContext<BtnContext>(locked_btn, {state, true}, [](lv_event_t* e) {
        auto* ctx = static_cast<BtnContext*>(lv_event_get_user_data(e));
        ctx->state->locked_btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
        executeLoad(ctx->state, ctx->with_locks);
    }, LV_EVENT_CLICKED);

    attachLvContext<DialogState*>(panel, state, [](lv_event_t* e) {
        auto* s = *static_cast<DialogState**>(lv_event_get_user_data(e));
        if (!s || !s->in_flight) return;
        if (s->backend->model().activePatch().id == s->target_patch && !s->backend->isBusy()) {
            lv_obj_delete(s->root);
            return;
        }
        if (s->backend->hasPatchError()) {
            s->in_flight = false;
            lv_obj_clear_state(s->clean_btn, LV_STATE_DISABLED);
            lv_obj_clear_state(s->locked_btn, LV_STATE_DISABLED);
            const char* notice = s->backend->statusText();
            lv_label_set_text(s->status, (notice && notice[0]) ? notice : "PATCH LOAD FAILED. RETRY.");
            lv_obj_set_style_text_color(s->status, lv_color_hex(palette::kWarning), 0);
            return;
        }
        if (lv_tick_elaps(s->request_start_ms) >= 2000) {
            s->in_flight = false;
            lv_obj_clear_state(s->clean_btn, LV_STATE_DISABLED);
            lv_obj_clear_state(s->locked_btn, LV_STATE_DISABLED);
            lv_label_set_text(s->status, "PATCH LOAD TIMEOUT. RETRY.");
            lv_obj_set_style_text_color(s->status, lv_color_hex(palette::kWarning), 0);
        }
    }, LV_EVENT_REFRESH);
}

void stepPatchWithConfirmation(IUiBackend& backend, UiNavigationManager& nav, int direction) {
    if (backend.isBusy()) return;
    size_t count = 0;
    const auto* cat = UiModel::catalog(count);
    const uint16_t cur_id = backend.model().activePatch().id;
    size_t cur_idx = 0;
    for (size_t i = 0; i < count; ++i) {
        if (cat[i].id == cur_id) { cur_idx = i; break; }
    }

    size_t next_idx = cur_idx;
    for (size_t step = 0; step < count; ++step) {
        next_idx = (next_idx + count + (direction > 0 ? 1 : count - 1)) % count;
        if (backend.isPatchSelectable(cat[next_idx].id)
#if defined(SMKP4_FULL_FACTORY_LIBRARY) || defined(CONFIG_SMKP4_FULL_FACTORY_LIBRARY)
            && backend.model().matchesBrowser(cat[next_idx])
#endif
           ) {
            break;
        }
    }
    if(next_idx==cur_idx || !backend.isPatchSelectable(cat[next_idx].id)) return;
#if defined(SMKP4_FULL_FACTORY_LIBRARY) || defined(CONFIG_SMKP4_FULL_FACTORY_LIBRARY)
    if(!backend.model().matchesBrowser(cat[next_idx]))return;
#endif
    selectPatchWithConfirmation(backend, nav, cat[next_idx].id, 0);
}

lv_obj_t* createPatchSelectorCard(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav,
                                  int32_t x, int32_t y, int32_t w, int32_t h,
                                  PresentationLevel level) {
    if (!parent) return nullptr;
    lv_obj_t* card = lv_obj_create(parent);
    if (!card) return nullptr;
    applyPanelStyle(card, true);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);

    const auto& patch = backend.model().activePatch();
    char title_buf[64];
    std::snprintf(title_buf, sizeof(title_buf), "%03u  %s", patch.id, patch.name);
    lv_obj_t* title_lbl=createLabel(card, title_buf, 8, 2, 338, &lv_font_montserrat_20, palette::kTextPrimary);

    lv_obj_t* subtitle=nullptr;
    char sub_buf[80];
    if (level == PresentationLevel::StageMinimal) {
        std::snprintf(sub_buf, sizeof(sub_buf), "%s  |  %s  |  %s",
                      patchCategoryName(patch.category),
                      synthFamilyName(patch.family),
                      patch.bank_name);
        subtitle=createLabel(card, sub_buf, 8, 30, 334, &lv_font_montserrat_14, palette::kAccentPrimary);
    } else {
        std::snprintf(sub_buf, sizeof(sub_buf), "%s | %s | %s",
                      patch.bank_name,
                      patchCategoryName(patch.category),
                      synthFamilyName(patch.family));
        subtitle=createLabel(card, sub_buf, 8, 30, 270, &lv_font_montserrat_14, palette::kAccentPrimary);
        createStatusBadge(card, backend.patchStatus(patch.id), 284, 30);
    }

    struct SubtitleCtx { IUiBackend* backend; PresentationLevel level; };
    attachLvContext<SubtitleCtx>(subtitle, SubtitleCtx{&backend,level}, [](lv_event_t* e) {
        auto* ctx=static_cast<SubtitleCtx*>(lv_event_get_user_data(e));
        const auto& p=ctx->backend->model().activePatch();
        char text[80];
        if(ctx->level==PresentationLevel::StageMinimal)
            std::snprintf(text,sizeof(text),"%s  |  %s  |  %s",patchCategoryName(p.category),synthFamilyName(p.family),p.bank_name);
        else std::snprintf(text,sizeof(text),"%s | %s | %s",p.bank_name,patchCategoryName(p.category),synthFamilyName(p.family));
        lv_label_set_text(static_cast<lv_obj_t*>(lv_event_get_target(e)),text);
    }, LV_EVENT_REFRESH);

    auto handler = [](lv_event_t* e) {
        if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
        auto* ctx = static_cast<PatchSelCtx*>(lv_event_get_user_data(e));
        if (!ctx || !ctx->backend || !ctx->nav) return;

        if (ctx->action == -1 || ctx->action == 1) {
            stepPatchWithConfirmation(*ctx->backend,*ctx->nav,ctx->action);
        } else if (ctx->action == 2) {
            ctx->nav->navigateTo(ScreenId::PatchBrowser);
        } else if (ctx->action == 3) {
            ctx->backend->model().toggleFavorite(ctx->backend->model().activePatch().id);
            ctx->nav->navigateTo(ctx->nav->currentScreen(), ctx->nav->subTab(ctx->nav->currentScreen()));
        } else if (ctx->action == 4) {
            ctx->nav->navigateTo(ScreenId::SoundEditor);
        }
    };

    const bool is_fav = backend.model().isFavorite(patch.id);
    const int32_t btn_h = h - 22;
    lv_obj_t* prev_btn = createActionButton(card, "< PREV",  348, 5, 86, btn_h, nullptr, nullptr);
    lv_obj_t* next_btn = createActionButton(card, "NEXT >",  440, 5, 86, btn_h, nullptr, nullptr);
    lv_obj_t* fav_btn  = createActionButton(card, is_fav ? "FAV *" : "FAV", 532, 5, 66, btn_h, nullptr, nullptr, is_fav, false);
    lv_obj_t* brw_btn  = createActionButton(card, "BROWSER", 604, 5, 96, btn_h, nullptr, nullptr, false, true);
    lv_obj_t* edt_btn  = createActionButton(card, "EDIT",    706, 5, 66, btn_h, nullptr, nullptr);

    if (prev_btn) attachLvContext<PatchSelCtx>(prev_btn, PatchSelCtx{&backend, &nav, -1}, handler, LV_EVENT_CLICKED);
    if (next_btn) attachLvContext<PatchSelCtx>(next_btn, PatchSelCtx{&backend, &nav, +1}, handler, LV_EVENT_CLICKED);
    if (fav_btn)  attachLvContext<PatchSelCtx>(fav_btn,  PatchSelCtx{&backend, &nav,  3}, handler, LV_EVENT_CLICKED);
    if (brw_btn)  attachLvContext<PatchSelCtx>(brw_btn,  PatchSelCtx{&backend, &nav,  2}, handler, LV_EVENT_CLICKED);
    if (edt_btn)  attachLvContext<PatchSelCtx>(edt_btn,  PatchSelCtx{&backend, &nav,  4}, handler, LV_EVENT_CLICKED);

    struct TitleCtx { IUiBackend* backend; };
    attachLvContext<TitleCtx>(title_lbl, TitleCtx{&backend}, [](lv_event_t* e) {
        auto* ctx=static_cast<TitleCtx*>(lv_event_get_user_data(e));
        const auto& patch=ctx->backend->model().activePatch();
        char text[64]; std::snprintf(text,sizeof(text),"%03u  %s",patch.id,patch.name);
        lv_label_set_text(static_cast<lv_obj_t*>(lv_event_get_target(e)),text);
    }, LV_EVENT_REFRESH);
    return card;
}

} // namespace smk::p4::ui
