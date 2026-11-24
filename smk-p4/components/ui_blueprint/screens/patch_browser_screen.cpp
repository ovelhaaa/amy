#include "ui_screens.h"
#include "ui_widgets.h"
#include <cstdio>
#include <cstring>

namespace smk::p4::ui {

namespace {

// Filter & pre-selection state persisted across screen switches
struct BrowserState {
    uint8_t       bank_filter       = 0; // 0=ALL, 1=JUNO A/ROM P4, 2=JUNO B/S3, 3=AMY DX7/USER, 4=FAVORITES
    PatchCategory cat_filter        = PatchCategory::All;
    uint8_t       page              = 0;
    uint8_t       target_layer      = 0; // 0=Layer A, 1=Layer B
    uint16_t      selected_patch_id = 0;
    bool          has_selection     = false;
};
BrowserState g_browser{};

struct BrowserActionCtx {
    IUiBackend*          backend = nullptr;
    UiNavigationManager* nav     = nullptr;
    uint8_t              kind    = 0;
    // 0=Bank, 1=CatStep(-1/+1), 2=SelectRow(preselect), 3=PagePrev, 4=PageNext,
    // 5=FavToggle, 6=LayerTarget, 7=UserPresets/DemoNote, 8=LoadWithDialog,
    // 9=QuickLoad, 10=GoToSoundEditor, 11=GoToPerformance
    int16_t              value   = 0;
};

bool matchesFilter(const IUiBackend& backend, const BlueprintPatchEntry& p) {
#if defined(SMKP4_FULL_FACTORY_LIBRARY) || defined(CONFIG_SMKP4_FULL_FACTORY_LIBRARY)
    return backend.model().matchesBrowser(p);
#else
    if (g_browser.bank_filter == 1 && backend.patchStatus(p.id) != FeatureStatus::Available) return false;
    if (g_browser.bank_filter == 2 && backend.patchStatus(p.id) != FeatureStatus::Planned) return false;
    if (g_browser.bank_filter == 3 && backend.patchStatus(p.id) != FeatureStatus::Demo) return false;
    if (g_browser.bank_filter == 4 && !backend.model().isFavorite(p.id)) return false;
    if (g_browser.cat_filter != PatchCategory::All && p.category != g_browser.cat_filter) return false;
    return true;
#endif
}

const BlueprintPatchEntry* findCatalogEntry(uint16_t patch_id) {
    size_t total = 0;
    const BlueprintPatchEntry* catalog = UiModel::catalog(total);
    for (size_t i = 0; i < total; ++i) {
        if (catalog[i].id == patch_id) return &catalog[i];
    }
    return total > 0 ? &catalog[0] : nullptr;
}

class PatchBrowserScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::PatchBrowser; }

    bool build(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav) override {
        backend.model().browser_bank = g_browser.bank_filter;
        backend.model().browser_category = g_browser.cat_filter;
        nav_ = &nav;
        shown_patch_ = backend.model().activePatch().id;
        last_loading_ = backend.isPatchLoading();
        last_error_ = backend.hasPatchError();
        if (!g_browser.has_selection || !findCatalogEntry(g_browser.selected_patch_id)) {
            g_browser.selected_patch_id = shown_patch_;
            g_browser.has_selection = true;
        }
        status_note_lbl_ = nullptr;
        load_btn_ = nullptr;
        quick_load_btn_ = nullptr;

        auto cb = [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
            auto* ctx = static_cast<BrowserActionCtx*>(lv_event_get_user_data(e));
            if (!ctx || !ctx->backend || !ctx->nav) return;
            switch (ctx->kind) {
                case 0:
                    g_browser.bank_filter = static_cast<uint8_t>(ctx->value);
                    g_browser.page = 0;
                    break;
                case 1: {
                    const int count = static_cast<int>(PatchCategory::Count);
                    const int cur = static_cast<int>(g_browser.cat_filter);
                    g_browser.cat_filter = static_cast<PatchCategory>((cur + ctx->value + count) % count);
                    g_browser.page = 0;
                    break;
                }
                case 2:
                    // Pre-selection only: never triggers audio patch load on first tap.
                    g_browser.selected_patch_id = static_cast<uint16_t>(ctx->value);
                    g_browser.has_selection = true;
                    break;
                case 3:
                    if (g_browser.page > 0) --g_browser.page;
                    break;
                case 4:
                    ++g_browser.page;
                    break;
                case 5:
                    ctx->backend->model().toggleFavorite(static_cast<uint16_t>(ctx->value));
                    break;
                case 6:
                    g_browser.target_layer = (g_browser.target_layer == 0) ? 1 : 0;
                    break;
                case 7: {
                    if (ctx->backend->hasSoundEditor()) {
                        showUserPresetPanel(static_cast<lv_obj_t*>(lv_event_get_current_target(e)), *ctx->backend);
                        return;
                    }
                    if (!ctx->backend->isDemoMode()) break;
                    const UiCommandResult r = ctx->backend->requestNoteOn(60, 100);
                    if (r.accepted()) ctx->backend->requestNoteOff(60, r.token);
                    break;
                }
                case 8:
                    selectPatchWithConfirmation(
                        *ctx->backend, *ctx->nav, g_browser.selected_patch_id, g_browser.target_layer, true);
                    return;
                case 9:
                    selectPatchWithConfirmation(
                        *ctx->backend, *ctx->nav, g_browser.selected_patch_id, g_browser.target_layer, false);
                    return;
                case 10:
                    ctx->nav->requestNavigateTo(ScreenId::SoundEditor);
                    return;
                case 11:
                    ctx->nav->requestReturnToPerformance();
                    return;
            }
            ctx->nav->requestNavigateTo(ScreenId::PatchBrowser);
        };

        // 1. Left Contextual Sidebar (x = 4..160, y = 4..384)
        lv_obj_t* sidebar = lv_obj_create(parent);
        applyPanelStyle(sidebar, false);
        lv_obj_set_pos(sidebar, 4, 4);
        lv_obj_set_size(sidebar, 156, 380);
        lv_obj_set_style_pad_all(sidebar, 4, 0);
        lv_obj_remove_flag(sidebar, LV_OBJ_FLAG_SCROLLABLE);

        createLabel(sidebar, "BANCOS", 4, 2, 140, &lv_font_montserrat_14, palette::kAccentPrimary);

        static constexpr const char* kBankLabels[5] = {
#if defined(SMKP4_FULL_FACTORY_LIBRARY) || defined(CONFIG_SMKP4_FULL_FACTORY_LIBRARY)
            "ALL BANKS", "JUNO A", "JUNO B", "AMY DX7", "FAVORITES *"
#else
            "ALL BANKS", "P4 ROM (AVAIL)", "S3/AMY (PLAN)", "USER (DEMO)", "FAVORITES *"
#endif
        };
        for (uint8_t b = 0; b < 5; ++b) {
            lv_obj_t* btn = createActionButton(
                sidebar, kBankLabels[b], 2, 22 + b * 38, 144, 34,
                nullptr, nullptr, g_browser.bank_filter == b, false);
            if (btn) {
                attachLvContext<BrowserActionCtx>(
                    btn, BrowserActionCtx{&backend, &nav, 0, b}, cb, LV_EVENT_CLICKED);
            }
        }

        // Category stepper inside sidebar
        createLabel(sidebar, "CATEGORIA", 4, 214, 140, &lv_font_montserrat_14, palette::kTextSecondary);
        lv_obj_t* cat_prev = createActionButton(sidebar, "<", 2, 234, 34, 34, nullptr, nullptr);
        lv_obj_t* cat_next = createActionButton(sidebar, ">", 112, 234, 34, 34, nullptr, nullptr);
        lv_obj_t* cat_lbl = createLabel(
            sidebar, patchCategoryName(g_browser.cat_filter), 38, 242, 72,
            &lv_font_montserrat_14, palette::kAccentPrimary);
        if (cat_lbl) lv_obj_set_style_text_align(cat_lbl, LV_TEXT_ALIGN_CENTER, 0);
        if (cat_prev) attachLvContext<BrowserActionCtx>(cat_prev, BrowserActionCtx{&backend, &nav, 1, -1}, cb, LV_EVENT_CLICKED);
        if (cat_next) attachLvContext<BrowserActionCtx>(cat_next, BrowserActionCtx{&backend, &nav, 1, 1}, cb, LV_EVENT_CLICKED);

        lv_obj_t* prv_btn = createActionButton(
            sidebar,
            backend.hasSoundEditor() ? "USER PRESETS / SAVE AS"
                                     : (backend.isDemoMode() ? "AUDITION DEMO" : "USER PRESETS"),
            2, 276, 144, 34, nullptr, nullptr, false, false);
        lv_obj_t* ed_btn = createActionButton(sidebar, "SINTESE >", 2, 314, 144, 30, nullptr, nullptr, false, false);
        lv_obj_t* pf_btn = createActionButton(sidebar, "TOCAR >", 2, 346, 144, 28, nullptr, nullptr, false, false);
        if (prv_btn) attachLvContext<BrowserActionCtx>(prv_btn, BrowserActionCtx{&backend, &nav, 7, 0}, cb, LV_EVENT_CLICKED);
        if (ed_btn) attachLvContext<BrowserActionCtx>(ed_btn, BrowserActionCtx{&backend, &nav, 10, 0}, cb, LV_EVENT_CLICKED);
        if (pf_btn) attachLvContext<BrowserActionCtx>(pf_btn, BrowserActionCtx{&backend, &nav, 11, 0}, cb, LV_EVENT_CLICKED);

        // 2. Center Paginated Patch List (x = 166..540, y = 4..384)
        size_t total_count = 0;
        const BlueprintPatchEntry* catalog = UiModel::catalog(total_count);
        constexpr size_t kRowsPerPage = 7;
        const BlueprintPatchEntry* filtered[kRowsPerPage]{};
        size_t match_count = 0;
        for (size_t i = 0; i < total_count; ++i) {
            if (matchesFilter(backend, catalog[i])) ++match_count;
        }

        const size_t total_pages = (match_count + kRowsPerPage - 1) / kRowsPerPage;
        if (g_browser.page >= total_pages && total_pages > 0) {
            g_browser.page = static_cast<uint8_t>(total_pages - 1);
        }

        size_t matched = 0, visible = 0;
        const size_t first = g_browser.page * kRowsPerPage;
        for (size_t i = 0; i < total_count && visible < kRowsPerPage; ++i) {
            if (matchesFilter(backend, catalog[i]) && matched++ >= first) {
                filtered[visible++] = &catalog[i];
            }
        }

        lv_obj_t* list_panel = lv_obj_create(parent);
        applyPanelStyle(list_panel, false);
        lv_obj_set_pos(list_panel, 166, 4);
        lv_obj_set_size(list_panel, 374, 380);
        lv_obj_set_style_pad_all(list_panel, 4, 0);
        lv_obj_remove_flag(list_panel, LV_OBJ_FLAG_SCROLLABLE);

        char hdr_txt[48];
        std::snprintf(hdr_txt, sizeof(hdr_txt), "TIMBRES (%u)  |  PAG %u/%u",
                      unsigned(match_count), unsigned(g_browser.page + 1),
                      unsigned(total_pages > 0 ? total_pages : 1));
        createLabel(list_panel, hdr_txt, 4, 2, 240, &lv_font_montserrat_14, palette::kTextSecondary);

        const uint16_t active_id = backend.model().layerPatch(g_browser.target_layer).id;
        for (size_t r = 0; r < kRowsPerPage; ++r) {
            const int32_t ry = 24 + static_cast<int32_t>(r) * 44;
            if (r >= visible) {
                createLabel(list_panel, "--- EMPTY ---", 8, ry + 10, 240,
                            &lv_font_montserrat_14, palette::kTextSecondary);
                continue;
            }
            const auto* entry = filtered[r];
            const bool is_active = (entry->id == active_id);
            const bool is_preselected = (entry->id == g_browser.selected_patch_id);
            const bool is_fav = backend.model().isFavorite(entry->id);
            const bool selectable = backend.isDemoMode() || backend.isPatchSelectable(entry->id);

            char row_txt[64];
            std::snprintf(row_txt, sizeof(row_txt), "%s%03u %.18s%s",
                          is_active ? "* " : "",
                          entry->id, entry->name,
                          is_fav ? " *" : "");
            lv_obj_t* rbtn = createActionButton(
                list_panel, row_txt, 4, ry, 294, 40,
                nullptr, nullptr, is_preselected, is_active && !is_preselected);
            if (rbtn) {
                if (is_active) {
                    lv_obj_set_style_border_width(rbtn, 2, 0);
                    lv_obj_set_style_border_color(rbtn, lv_color_hex(palette::kActive), 0);
                }
                if (!selectable) {
                    lv_obj_set_style_text_color(rbtn, lv_color_hex(palette::kTextSecondary), 0);
                }
                attachLvContext<BrowserActionCtx>(
                    rbtn, BrowserActionCtx{&backend, &nav, 2, int16_t(entry->id)}, cb, LV_EVENT_CLICKED);
            }
            createStatusBadge(list_panel, backend.patchStatus(entry->id), 302, ry + 10);
        }

        lv_obj_t* pprev = createActionButton(list_panel, "< PAGE", 182, 336, 86, 34, nullptr, nullptr);
        lv_obj_t* pnext = createActionButton(list_panel, "PAGE >", 274, 336, 86, 34, nullptr, nullptr);
        if (pprev) attachLvContext<BrowserActionCtx>(pprev, BrowserActionCtx{&backend, &nav, 3, 0}, cb, LV_EVENT_CLICKED);
        if (pnext) attachLvContext<BrowserActionCtx>(pnext, BrowserActionCtx{&backend, &nav, 4, 0}, cb, LV_EVENT_CLICKED);
        createLabel(list_panel, "* = ATIVO", 6, 344, 160, &lv_font_montserrat_14, palette::kActive);

        // 3. Right Inspector & Load Actions Panel (x = 546..794, y = 4..384)
        lv_obj_t* insp = lv_obj_create(parent);
        applyPanelStyle(insp, true);
        lv_obj_set_pos(insp, 546, 4);
        lv_obj_set_size(insp, 248, 380);
        lv_obj_set_style_pad_all(insp, 6, 0);
        lv_obj_remove_flag(insp, LV_OBJ_FLAG_SCROLLABLE);

        const BlueprintPatchEntry* sel = findCatalogEntry(g_browser.selected_patch_id);
        const BlueprintPatchEntry& cur = sel ? *sel : backend.model().activePatch();
        const bool sel_is_active = (cur.id == active_id);
        const bool sel_selectable = backend.isDemoMode() || (backend.isPatchSelectable(cur.id) && g_browser.target_layer == 0);

        createLabel(insp, sel_is_active ? "PATCH ATIVO" : "PRE-SELECAO", 4, 2, 160,
                    &lv_font_montserrat_14, sel_is_active ? palette::kActive : palette::kAccentPrimary);
        createStatusBadge(insp, backend.patchStatus(cur.id), 174, 2);

        char name_line[48];
        std::snprintf(name_line, sizeof(name_line), "%03u %s", cur.id, cur.name);
        lv_obj_t* name_lbl = createLabel(insp, name_line, 4, 24, 228, &lv_font_montserrat_20, palette::kTextPrimary);
        if (name_lbl) lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_MODE_DOTS);

        char meta1[64];
        std::snprintf(meta1, sizeof(meta1), "BANCO: %s | %s", cur.bank_name, patchCategoryName(cur.category));
        createLabel(insp, meta1, 4, 54, 232, &lv_font_montserrat_14, palette::kTextSecondary);

        char meta2[64];
        std::snprintf(meta2, sizeof(meta2), "MOTOR: %s", synthFamilyName(cur.family));
        createLabel(insp, meta2, 4, 76, 232, &lv_font_montserrat_14, palette::kActive);

        char macros_line[80];
        std::snprintf(macros_line, sizeof(macros_line), "M1-4: %s|%s|%s|%s",
                      cur.macro_labels[0], cur.macro_labels[1], cur.macro_labels[2], cur.macro_labels[3]);
        lv_obj_t* mac_lbl = createLabel(insp, macros_line, 4, 98, 232, &lv_font_montserrat_14, palette::kTextPrimary);
        if (mac_lbl) lv_label_set_long_mode(mac_lbl, LV_LABEL_LONG_MODE_DOTS);

        status_note_lbl_ = createLabel(insp, "", 4, 140, 232, &lv_font_montserrat_14, palette::kWarning);
        if (status_note_lbl_) lv_label_set_long_mode(status_note_lbl_, LV_LABEL_LONG_MODE_DOTS);

        load_btn_ = createActionButton(
            insp, "CARREGAR", 4, 174, 112, 44, nullptr, nullptr, false, sel_selectable);
        quick_load_btn_ = createActionButton(
            insp, "CARREGAR JA", 122, 174, 112, 44, nullptr, nullptr, false, false);

        lv_obj_t* fav_btn = createActionButton(
            insp, backend.model().isFavorite(cur.id) ? "FAVORITE: YES *" : "FAVORITE: NO",
            4, 226, 112, 36, nullptr, nullptr, backend.model().isFavorite(cur.id), false);
        lv_obj_t* tgt_btn = createActionButton(
            insp, g_browser.target_layer == 0 ? "LAYER A" : "LAYER B",
            122, 226, 112, 36, nullptr, nullptr, false, false);

        lv_obj_t* edit_btn = createActionButton(
            insp, "ABRIR SINTESE >", 4, 272, 230, 38, nullptr, nullptr, false, false);
        lv_obj_t* stage_btn = createActionButton(
            insp, "VOLTAR P/ TOCAR >", 4, 318, 230, 38, nullptr, nullptr, false, false);

        if (load_btn_) attachLvContext<BrowserActionCtx>(load_btn_, BrowserActionCtx{&backend, &nav, 8, int16_t(cur.id)}, cb, LV_EVENT_CLICKED);
        if (quick_load_btn_) attachLvContext<BrowserActionCtx>(quick_load_btn_, BrowserActionCtx{&backend, &nav, 9, int16_t(cur.id)}, cb, LV_EVENT_CLICKED);
        if (fav_btn) attachLvContext<BrowserActionCtx>(fav_btn, BrowserActionCtx{&backend, &nav, 5, int16_t(cur.id)}, cb, LV_EVENT_CLICKED);
        if (tgt_btn) attachLvContext<BrowserActionCtx>(tgt_btn, BrowserActionCtx{&backend, &nav, 6, 0}, cb, LV_EVENT_CLICKED);
        if (edit_btn) attachLvContext<BrowserActionCtx>(edit_btn, BrowserActionCtx{&backend, &nav, 10, 0}, cb, LV_EVENT_CLICKED);
        if (stage_btn) attachLvContext<BrowserActionCtx>(stage_btn, BrowserActionCtx{&backend, &nav, 11, 0}, cb, LV_EVENT_CLICKED);

        updateInspectorStatus(backend);
        return true;
    }

    void onDestroy() override {
        closeUserPresetPanel(false);
    }

    void refresh(IUiBackend& backend) override {
        if (nav_ && shown_patch_ != backend.model().activePatch().id) {
            nav_->requestNavigateTo(ScreenId::PatchBrowser);
            return;
        }
        updateInspectorStatus(backend);
        last_loading_ = backend.isPatchLoading();
        last_error_ = backend.hasPatchError();
    }

private:
    void updateInspectorStatus(IUiBackend& backend) {
        const BlueprintPatchEntry* sel = findCatalogEntry(g_browser.selected_patch_id);
        const BlueprintPatchEntry& cur = sel ? *sel : backend.model().activePatch();
        const bool sel_selectable = backend.isDemoMode() || (backend.isPatchSelectable(cur.id) && g_browser.target_layer == 0);
        const bool loading = backend.isPatchLoading();
        const bool busy = backend.isBusy();

        if (status_note_lbl_) {
            const char* note = "";
            uint32_t col = palette::kTextSecondary;
            if (loading) {
                note = "CARREGANDO PATCH...";
                col = palette::kAccentPrimary;
            } else if (backend.hasPatchError()) {
                const auto res = backend.patchLoadResult();
                note = (res.notice && res.notice[0]) ? res.notice : "FALHA AO CARREGAR";
                col = palette::kErrorPanic;
            } else if (!backend.isDemoMode() && g_browser.target_layer != 0) {
                note = "LAYER B PLANNED (SEM DSP)";
                col = palette::kWarning;
            } else if (backend.patchNeedsDiscardConfirmation()) {
                note = "EDICOES NAO SALVAS (CONFIRMA)";
                col = palette::kWarning;
            } else if (backend.patchStatus(cur.id) == FeatureStatus::Available) {
                note = (cur.id == backend.model().activePatch().id) ? "PATCH ATIVO NO ENGINE" : "PRONTO P/ CARREGAR";
                col = palette::kActive;
            } else if (backend.patchStatus(cur.id) == FeatureStatus::Planned) {
                note = "INDISPONIVEL NO HOST P4";
                col = palette::kWarning;
            } else {
                note = "DEMO SLOT (RAM)";
                col = palette::kWarning;
            }
            if (std::strcmp(lv_label_get_text(status_note_lbl_), note) != 0) {
                lv_label_set_text(status_note_lbl_, note);
            }
            lv_obj_set_style_text_color(status_note_lbl_, lv_color_hex(col), 0);
        }

        const bool disable_load = !sel_selectable || loading || busy;
        if (load_btn_) {
            if (disable_load) lv_obj_add_state(load_btn_, LV_STATE_DISABLED);
            else lv_obj_remove_state(load_btn_, LV_STATE_DISABLED);
        }
        if (quick_load_btn_) {
            if (disable_load) lv_obj_add_state(quick_load_btn_, LV_STATE_DISABLED);
            else lv_obj_remove_state(quick_load_btn_, LV_STATE_DISABLED);
        }
    }

    UiNavigationManager* nav_ = nullptr;
    uint16_t shown_patch_ = 0;
    bool last_loading_ = false;
    bool last_error_ = false;
    lv_obj_t* status_note_lbl_ = nullptr;
    lv_obj_t* load_btn_ = nullptr;
    lv_obj_t* quick_load_btn_ = nullptr;
};

} // namespace

ScreenView* createPatchBrowserScreen() {
    return new PatchBrowserScreen();
}

} // namespace smk::p4::ui
