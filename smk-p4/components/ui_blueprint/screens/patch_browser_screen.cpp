#include "ui_screens.h"
#include "ui_widgets.h"
#include <cstdio>
#include <cstring>

namespace smk::p4::ui {

namespace {

// Filter state persisted in static module struct so category/bank/page survive screen switches
struct BrowserState {
    uint8_t       bank_filter = 0; // 0=ALL, 1=ROM P4, 2=ROM S3/AMY, 3=USER DEMO, 4=FAVORITES
    PatchCategory cat_filter  = PatchCategory::All;
    uint8_t       page        = 0;
    uint8_t       target_layer= 0; // 0=Layer A, 1=Layer B
};
BrowserState g_browser{};

struct BrowserActionCtx {
    IUiBackend*          backend = nullptr;
    UiNavigationManager* nav     = nullptr;
    uint8_t              kind    = 0; // 0=Bank, 1=Category, 2=SelectPatch, 3=PagePrev, 4=PageNext, 5=FavToggle, 6=LayerTarget, 7=PreviewNote
    uint16_t             value   = 0;
};

bool matchesFilter(const UiModel& model, const BlueprintPatchEntry& p) {
    if (g_browser.bank_filter == 1 && p.status != FeatureStatus::Available) return false;
    if (g_browser.bank_filter == 2 && p.status != FeatureStatus::Planned) return false;
    if (g_browser.bank_filter == 3 && p.status != FeatureStatus::Demo) return false;
    if (g_browser.bank_filter == 4 && !model.isFavorite(p.id)) return false;
    if (g_browser.cat_filter != PatchCategory::All && p.category != g_browser.cat_filter) return false;
    return true;
}

class PatchBrowserScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::PatchBrowser; }

    bool build(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav) override {
        nav_=&nav; shown_patch_=backend.model().activePatch().id;
        auto cb = [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
            auto* ctx = static_cast<BrowserActionCtx*>(lv_event_get_user_data(e));
            if (!ctx || !ctx->backend || !ctx->nav) return;
            switch (ctx->kind) {
                case 0:
                    g_browser.bank_filter = static_cast<uint8_t>(ctx->value);
                    g_browser.page = 0;
                    break;
                case 1:
                    g_browser.cat_filter = static_cast<PatchCategory>(ctx->value);
                    g_browser.page = 0;
                    break;
                case 2:
                    ctx->backend->requestSelectPatch(ctx->value, g_browser.target_layer);
                    break;
                case 3:
                    if (g_browser.page > 0) --g_browser.page;
                    break;
                case 4:
                    ++g_browser.page;
                    break;
                case 5:
                    ctx->backend->model().toggleFavorite(ctx->value);
                    break;
                case 6:
                    g_browser.target_layer = (g_browser.target_layer == 0) ? 1 : 0;
                    break;
                case 7: {
                    if (!ctx->backend->isDemoMode()) break;
                    const UiCommandResult r = ctx->backend->requestNoteOn(60, 100);
                    if (r.accepted()) {
                        ctx->backend->requestNoteOff(60, r.token);
                    }
                    break;
                }
            }
            ctx->nav->navigateTo(ScreenId::PatchBrowser);
        };

        // 1. Bank Filter Row (y = 4..40)
        static constexpr const char* kBankLabels[5] = {
            "ALL BANKS", "P4 ROM (AVAIL)", "S3/AMY ROM (PLAN)", "USER (DEMO)", "FAVORITES *"
        };
        for (uint8_t b = 0; b < 5; ++b) {
            lv_obj_t* btn = createActionButton(parent, kBankLabels[b], 8 + b * 158, 4, 152, 34,
                                               nullptr, nullptr, g_browser.bank_filter == b, false);
            if (btn) attachLvContext<BrowserActionCtx>(btn, BrowserActionCtx{&backend, &nav, 0, b}, cb, LV_EVENT_CLICKED);
        }

        // 2. Category Pills Row (y = 42..78) - 12 categories across 784px
        for (uint8_t c = 0; c < static_cast<uint8_t>(PatchCategory::Count); ++c) {
            const auto cat = static_cast<PatchCategory>(c);
            const int32_t cx = 8 + c * 65;
            lv_obj_t* btn = createActionButton(parent, patchCategoryName(cat), cx, 42, 62, 34,
                                               nullptr, nullptr, g_browser.cat_filter == cat, false);
            if (btn) attachLvContext<BrowserActionCtx>(btn, BrowserActionCtx{&backend, &nav, 1, c}, cb, LV_EVENT_CLICKED);
        }

        // Collect matching patches
        size_t total_count = 0;
        const BlueprintPatchEntry* catalog = UiModel::catalog(total_count);
        const BlueprintPatchEntry* filtered[UiModel::kCatalogSize]{};
        size_t match_count = 0;
        for (size_t i = 0; i < total_count; ++i) {
            if (matchesFilter(backend.model(), catalog[i])) {
                filtered[match_count++] = &catalog[i];
            }
        }

        constexpr size_t kRowsPerPage = 6;
        const size_t total_pages = (match_count + kRowsPerPage - 1) / kRowsPerPage;
        if (g_browser.page >= total_pages && total_pages > 0) {
            g_browser.page = static_cast<uint8_t>(total_pages - 1);
        }

        // 3. Left List Panel (x = 8..516, y = 82..386)
        lv_obj_t* list_panel = lv_obj_create(parent);
        applyPanelStyle(list_panel, false);
        lv_obj_set_pos(list_panel, 8, 82);
        lv_obj_set_size(list_panel, 508, 304);

        const uint16_t active_id = backend.model().layerPatch(g_browser.target_layer).id;
        for (size_t r = 0; r < kRowsPerPage; ++r) {
            const size_t idx = g_browser.page * kRowsPerPage + r;
            const int32_t ry = 4 + static_cast<int32_t>(r) * 42;
            if (idx >= match_count) {
                createLabel(list_panel, "--- EMPTY SLOT ---", 12, ry + 8, 300,
                            &lv_font_montserrat_14, palette::kTextSecondary);
                continue;
            }
            const auto* entry = filtered[idx];
            const bool is_sel = (entry->id == active_id);
            const bool is_fav = backend.model().isFavorite(entry->id);

            char row_txt[64];
            std::snprintf(row_txt, sizeof(row_txt), "%03u  %-18s  [%s] %s",
                          entry->id, entry->name, entry->bank_name, is_fav ? "*" : "");
            lv_obj_t* rbtn = createActionButton(list_panel, row_txt, 4, ry, 416, 38,
                                                nullptr, nullptr, is_sel, false);
            if (rbtn) {
                attachLvContext<BrowserActionCtx>(rbtn, BrowserActionCtx{&backend, &nav, 2, entry->id}, cb, LV_EVENT_CLICKED);
            }
            createStatusBadge(list_panel, entry->status, 426, ry + 10);
        }

        // Pagination footer inside list panel
        char page_txt[48];
        std::snprintf(page_txt, sizeof(page_txt), "MATCHES: %u   PAGE %u/%u",
                      unsigned(match_count), unsigned(g_browser.page + 1),
                      unsigned(total_pages > 0 ? total_pages : 1));
        createLabel(list_panel, page_txt, 8, 262, 240, &lv_font_montserrat_14, palette::kTextSecondary);

        lv_obj_t* pprev = createActionButton(list_panel, "< PAGE", 296, 256, 94, 36, nullptr, nullptr);
        lv_obj_t* pnext = createActionButton(list_panel, "PAGE >", 398, 256, 94, 36, nullptr, nullptr);
        if (pprev) attachLvContext<BrowserActionCtx>(pprev, BrowserActionCtx{&backend, &nav, 3, 0}, cb, LV_EVENT_CLICKED);
        if (pnext) attachLvContext<BrowserActionCtx>(pnext, BrowserActionCtx{&backend, &nav, 4, 0}, cb, LV_EVENT_CLICKED);

        // 4. Right Inspector & Demo Preview Panel (x = 522..792, y = 82..386)
        lv_obj_t* insp = lv_obj_create(parent);
        applyPanelStyle(insp, true);
        lv_obj_set_pos(insp, 522, 82);
        lv_obj_set_size(insp, 270, 304);

        const auto& cur = backend.model().layerPatch(g_browser.target_layer);
        createLabel(insp, "PATCH INSPECTOR", 8, 4, 180, &lv_font_montserrat_14, palette::kAccentPrimary);
        createStatusBadge(insp, cur.status, 190, 4);

        char name_line[48];
        std::snprintf(name_line, sizeof(name_line), "%03u %s", cur.id, cur.name);
        createLabel(insp, name_line, 8, 28, 246, &lv_font_montserrat_20, palette::kTextPrimary);

        char meta1[64];
        std::snprintf(meta1, sizeof(meta1), "BANK: %s   CAT: %s", cur.bank_name, patchCategoryName(cur.category));
        createLabel(insp, meta1, 8, 60, 246, &lv_font_montserrat_14, palette::kTextSecondary);

        char meta2[64];
        std::snprintf(meta2, sizeof(meta2), "ENGINE: %s", synthFamilyName(cur.family));
        createLabel(insp, meta2, 8, 82, 246, &lv_font_montserrat_14, palette::kActive);

        char macros_line[80];
        std::snprintf(macros_line, sizeof(macros_line), "M1-4: %s|%s|%s|%s",
                      cur.macro_labels[0], cur.macro_labels[1], cur.macro_labels[2], cur.macro_labels[3]);
        createLabel(insp, macros_line, 8, 104, 254, &lv_font_montserrat_14, palette::kTextPrimary);

        const char* status_note =
            (!backend.isDemoMode() && g_browser.target_layer != 0) ? "LAYER B PLANNED; NO DSP" :
            (cur.status == FeatureStatus::Available) ? "P4 RUNTIME: AMY ROM READY"
          : (cur.status == FeatureStatus::Planned)   ? "S3/AMY ROM: PLANNED FOR P4"
                                                     : "DEMO SLOT: UI RAM PROTOTYPE";
        createLabel(insp, status_note, 8, 128, 246, &lv_font_montserrat_14, palette::kWarning);

        lv_obj_t* tgt_btn = createActionButton(
            insp, g_browser.target_layer == 0 ? "TARGET: LAYER A" : "TARGET: LAYER B",
            8, 160, 242, 38, nullptr, nullptr, false, false);
        lv_obj_t* fav_btn = createActionButton(
            insp, backend.model().isFavorite(cur.id) ? "FAVORITE: YES *" : "FAVORITE: NO",
            8, 204, 242, 38, nullptr, nullptr, backend.model().isFavorite(cur.id), false);
        lv_obj_t* prv_btn = createActionButton(
            insp, backend.isDemoMode() ? "AUDITION DEMO NOTE (RAM)" : "AUDITION: USE STAGE KEYS",
            8, 248, 242, 38, nullptr, nullptr, false, true);

        if (tgt_btn) attachLvContext<BrowserActionCtx>(tgt_btn, BrowserActionCtx{&backend, &nav, 6, 0}, cb, LV_EVENT_CLICKED);
        if (fav_btn) attachLvContext<BrowserActionCtx>(fav_btn, BrowserActionCtx{&backend, &nav, 5, cur.id}, cb, LV_EVENT_CLICKED);
        if (prv_btn) attachLvContext<BrowserActionCtx>(prv_btn, BrowserActionCtx{&backend, &nav, 7, cur.id}, cb, LV_EVENT_CLICKED);

        return true;
    }

    void refresh(IUiBackend& backend) override {
        if(nav_ && shown_patch_!=backend.model().activePatch().id) nav_->requestNavigateTo(ScreenId::PatchBrowser);
    }
private:
    UiNavigationManager* nav_=nullptr;
    uint16_t shown_patch_=0;
};

} // namespace

ScreenView* createPatchBrowserScreen() {
    return new PatchBrowserScreen();
}

} // namespace smk::p4::ui
