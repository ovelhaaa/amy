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
    createLabel(card, title_buf, 8, 2, 338, &lv_font_montserrat_20, palette::kTextPrimary);

    char sub_buf[80];
    if (level == PresentationLevel::StageMinimal) {
        std::snprintf(sub_buf, sizeof(sub_buf), "%s  |  %s  |  %s",
                      patchCategoryName(patch.category),
                      synthFamilyName(patch.family),
                      patch.bank_name);
        createLabel(card, sub_buf, 8, 30, 334, &lv_font_montserrat_14, palette::kAccentPrimary);
    } else {
        std::snprintf(sub_buf, sizeof(sub_buf), "%s | %s | %s",
                      patch.bank_name,
                      patchCategoryName(patch.category),
                      synthFamilyName(patch.family));
        createLabel(card, sub_buf, 8, 30, 270, &lv_font_montserrat_14, palette::kAccentPrimary);
        createStatusBadge(card, patch.status, 284, 30);
    }

    auto handler = [](lv_event_t* e) {
        if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
        auto* ctx = static_cast<PatchSelCtx*>(lv_event_get_user_data(e));
        if (!ctx || !ctx->backend || !ctx->nav) return;

        size_t count = 0;
        const auto* cat = UiModel::catalog(count);
        const uint16_t cur_id = ctx->backend->model().activePatch().id;
        size_t cur_idx = 0;
        for (size_t i = 0; i < count; ++i) {
            if (cat[i].id == cur_id) { cur_idx = i; break; }
        }

        if (ctx->action == -1 || ctx->action == 1) {
            size_t next_idx = cur_idx;
            for (size_t step = 0; step < count; ++step) {
                next_idx = (next_idx + count + (ctx->action > 0 ? 1 : count - 1)) % count;
                if (ctx->backend->isPatchSelectable(cat[next_idx].id)) {
                    break;
                }
            }
            ctx->backend->requestSelectPatch(cat[next_idx].id, 0);
            ctx->nav->navigateTo(ctx->nav->currentScreen(), ctx->nav->subTab(ctx->nav->currentScreen()));
        } else if (ctx->action == 2) {
            ctx->nav->navigateTo(ScreenId::PatchBrowser);
        } else if (ctx->action == 3) {
            ctx->backend->model().toggleFavorite(cur_id);
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

    return card;
}

} // namespace smk::p4::ui
