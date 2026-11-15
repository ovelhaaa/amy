#include "ui_widgets.h"
#include <cstdio>

namespace smk::p4::ui {

namespace {
struct PatchSelCtx {
    IUiBackend*          backend = nullptr;
    UiNavigationManager* nav     = nullptr;
    int                  action  = 0; // -1=Prev, +1=Next, 2=Browse, 3=Fav, 4=SoundEdit
};
} // namespace

void selectPatchWithConfirmation(IUiBackend& backend, UiNavigationManager& nav, uint16_t patch, uint8_t layer) {
    if (layer || !backend.patchNeedsDiscardConfirmation()) {
        backend.requestSelectPatch(patch, layer);
        return;
    }
    static lv_obj_t* confirmation=nullptr;
    if(confirmation) return;
    auto* root = lv_obj_create(nav.contentRoot());
    if (!root) return;
    confirmation=root;
    lv_obj_add_event_cb(root, [](lv_event_t*) { confirmation=nullptr; }, LV_EVENT_DELETE, nullptr);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_70, 0);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    auto* panel = lv_obj_create(root);
    applyPanelStyle(panel, true);
    lv_obj_set_size(panel, 550, 180);
    lv_obj_center(panel);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    char heading[64];
    std::snprintf(heading, sizeof(heading), "LOAD PATCH %03u?", unsigned(patch));
    createLabel(panel, heading, 12, 10, 510);
    createLabel(panel, "Unsaved changes to the current patch will be lost.", 12, 43, 510);
    auto* status = createLabel(panel, "", 12, 70, 510);
    auto* cancel = createActionButton(panel, "CANCEL", 12, 102, 150, 44, nullptr, nullptr);
    auto* confirm = createActionButton(panel, "DISCARD & LOAD", 174, 102, 220, 44, nullptr, nullptr);
    lv_obj_add_event_cb(cancel, [](lv_event_t* event) {
        lv_obj_delete(static_cast<lv_obj_t*>(lv_event_get_user_data(event)));
    }, LV_EVENT_CLICKED, root);
    struct Context { IUiBackend* backend; lv_obj_t* root; lv_obj_t* status; uint16_t patch; uint16_t original; };
    attachLvContext<Context>(confirm, {&backend, root, status, patch, backend.model().activePatch().id},
        [](lv_event_t* event) {
            const auto& ctx = *static_cast<Context*>(lv_event_get_user_data(event));
            if (ctx.original != ctx.backend->model().activePatch().id) {
                lv_label_set_text(ctx.status, "Current patch changed. Cancel and select again.");
                return;
            }
            const auto result = ctx.backend->confirmPatchDiscard(ctx.patch, 0);
            if (result.accepted()) lv_obj_delete(ctx.root);
            else lv_label_set_text(ctx.status, result.notice ? result.notice : "Patch change unavailable.");
        }, LV_EVENT_CLICKED);
}

void stepPatchWithConfirmation(IUiBackend& backend, UiNavigationManager& nav, int direction) {
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
