#include "ui_widgets.h"
#include "fm_topology.h"
#include <cstdio>
#include <cstring>

namespace smk::p4::ui {

namespace {

struct AlgoSelectorCtx {
    IUiBackend* backend = nullptr;
    lv_obj_t*   card = nullptr;
    lv_obj_t*   readout_btn = nullptr;
    lv_obj_t*   readout_lbl = nullptr;
    lv_obj_t*   status_lbl = nullptr;
    lv_obj_t*   prev_btn = nullptr;
    lv_obj_t*   next_btn = nullptr;
};

void refreshSelector(AlgoSelectorCtx* ctx) {
    if (!ctx || !ctx->backend) return;
    const bool is_fm = (ctx->backend->model().activePatch().family == SynthFamily::FM);
    const int current_algo = ctx->backend->model().getInt("fm.algo", 1);
    const char* edit_status = ctx->backend->soundEditStatus();

    char abuf[32];
    std::snprintf(abuf, sizeof(abuf), "ALGO %02d / 32", current_algo);
    if (ctx->readout_lbl) lv_label_set_text(ctx->readout_lbl, abuf);

    char sbuf[48];
    if (!is_fm) {
        std::snprintf(sbuf, sizeof(sbuf), "UNAVAILABLE");
        if (ctx->status_lbl) {
            lv_label_set_text(ctx->status_lbl, sbuf);
            lv_obj_set_style_text_color(ctx->status_lbl, lv_color_hex(palette::kTextSecondary), 0);
        }
    } else {
        std::snprintf(sbuf, sizeof(sbuf), "%s", edit_status ? edit_status : "ACTIVE");
        if (ctx->status_lbl) {
            lv_label_set_text(ctx->status_lbl, sbuf);
            const bool dirty = edit_status && std::strstr(edit_status, "UNSAVED") != nullptr;
            lv_obj_set_style_text_color(ctx->status_lbl, lv_color_hex(dirty ? palette::kWarning : palette::kTextSecondary), 0);
        }
    }

    if (ctx->prev_btn) {
        if (!is_fm || current_algo <= 1) lv_obj_add_state(ctx->prev_btn, LV_STATE_DISABLED);
        else lv_obj_clear_state(ctx->prev_btn, LV_STATE_DISABLED);
    }
    if (ctx->next_btn) {
        if (!is_fm || current_algo >= 32) lv_obj_add_state(ctx->next_btn, LV_STATE_DISABLED);
        else lv_obj_clear_state(ctx->next_btn, LV_STATE_DISABLED);
    }
    if (ctx->readout_btn) {
        if (!is_fm) lv_obj_add_state(ctx->readout_btn, LV_STATE_DISABLED);
        else lv_obj_clear_state(ctx->readout_btn, LV_STATE_DISABLED);
    }
}

struct TopologyDiagramCtx {
    IUiBackend* backend = nullptr;
    lv_obj_t*   panel = nullptr;
    lv_obj_t*   title_lbl = nullptr;
    lv_obj_t*   op_btns[6] = {};
    lv_obj_t*   op_lbls[6] = {};
    lv_obj_t*   op_roles[6] = {};
};

void refreshTopology(TopologyDiagramCtx* ctx) {
    if (!ctx || !ctx->backend) return;
    const bool is_fm = (ctx->backend->model().activePatch().family == SynthFamily::FM);
    const int current_algo = ctx->backend->model().getInt("fm.algo", 1);
    const int selected_op = ctx->backend->model().getInt("fm.selected_op", 1);

    if (!is_fm) {
        if (ctx->title_lbl) lv_label_set_text(ctx->title_lbl, "NON-FM PATCH SELECTED: 6-OP TOPOLOGY UNAVAILABLE");
        for (unsigned i = 0; i < 6; ++i) {
            if (ctx->op_btns[i]) lv_obj_add_state(ctx->op_btns[i], LV_STATE_DISABLED);
        }
        return;
    }

    const auto topo = smk::p4::fm::deriveTopology(current_algo);
    char fbuf[128];
    smk::p4::fm::formatRoutingFormula(topo, fbuf, sizeof(fbuf));

    char tbuf[160];
    std::snprintf(tbuf, sizeof(tbuf), "TOPOLOGY: ALGO %02d / 32  |  %s", current_algo, fbuf);
    if (ctx->title_lbl) lv_label_set_text(ctx->title_lbl, tbuf);

    for (unsigned op = 1; op <= 6; ++op) {
        const unsigned idx = op - 1;
        lv_obj_t* btn = ctx->op_btns[idx];
        if (!btn) continue;
        lv_obj_clear_state(btn, LV_STATE_DISABLED);

        const bool is_carrier = topo.isCarrier(op);
        const bool is_selected = (selected_op == int(op));
        const bool has_fb = topo.hasFeedback(op);

        const uint32_t color = is_carrier ? palette::kActive : palette::kAccentPrimary;
        lv_obj_set_style_border_color(btn, lv_color_hex(is_selected ? palette::kTextPrimary : color), 0);
        lv_obj_set_style_border_width(btn, is_selected ? 3 : 1, 0);

        char op_name[20];
        std::snprintf(op_name, sizeof(op_name), "OP %u%s", op, has_fb ? " [FB]" : "");
        if (ctx->op_lbls[idx]) {
            lv_label_set_text(ctx->op_lbls[idx], op_name);
            lv_obj_set_style_text_color(ctx->op_lbls[idx], lv_color_hex(is_selected ? palette::kTextPrimary : color), 0);
        }

        if (ctx->op_roles[idx]) {
            lv_label_set_text(ctx->op_roles[idx], is_carrier ? "CARRIER" : "MOD");
            lv_obj_set_style_text_color(ctx->op_roles[idx], lv_color_hex(color), 0);
        }
    }
}

} // namespace

void showFmAlgorithmPicker(lv_obj_t* anchor, IUiBackend& backend) {
    lv_obj_t* screen = anchor ? lv_obj_get_screen(anchor) : lv_screen_active();
    if (!screen) screen = lv_screen_active();
    if (!screen) return;

    lv_obj_t* root = lv_obj_create(screen);
    if (!root) return;
    applyPanelStyle(root, true);
    lv_obj_set_pos(root, 60, 48);
    lv_obj_set_size(root, 680, 384);

    createLabel(root, "SELECT FM ALGORITHM (1 - 32)", 16, 8, 480, &lv_font_montserrat_14, palette::kAccentPrimary);
    createLabel(root, "Touch an algorithm number to modify topology across all 8 voices", 16, 28, 480, &lv_font_montserrat_14, palette::kTextSecondary);

    const int current_algo = backend.model().getInt("fm.algo", 1);

    struct ModalCtx {
        IUiBackend* backend;
        lv_obj_t* root;
    };
    auto* mctx = attachLvContext<ModalCtx>(root, ModalCtx{&backend, root}, nullptr);

    // 4 rows of 8 buttons = 32 algorithms
    const int start_x = 16, start_y = 56;
    const int btn_w = 74, btn_h = 64;
    const int gap_x = 8, gap_y = 8;

    for (int i = 1; i <= 32; ++i) {
        const int col = (i - 1) % 8;
        const int row = (i - 1) / 8;
        const int bx = start_x + col * (btn_w + gap_x);
        const int by = start_y + row * (btn_h + gap_y);

        char num_str[16];
        if (i == current_algo) {
            std::snprintf(num_str, sizeof(num_str), "%02d\nACT", i);
        } else {
            std::snprintf(num_str, sizeof(num_str), "%02d", i);
        }

        lv_obj_t* btn = createActionButton(root, num_str, bx, by, btn_w, btn_h, nullptr, nullptr, (i == current_algo));
        if (i == current_algo) {
            lv_obj_set_style_border_color(btn, lv_color_hex(palette::kActive), 0);
            lv_obj_set_style_border_width(btn, 2, 0);
        }

        struct BtnCtx {
            ModalCtx* m;
            int algo;
        };
        attachLvContext<BtnCtx>(btn, BtnCtx{mctx, i}, [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
            auto* bctx = static_cast<BtnCtx*>(lv_event_get_user_data(e));
            if (!bctx || !bctx->m || !bctx->m->backend) return;
            bctx->m->backend->requestParameterInt("fm.algo", bctx->algo);
            lv_obj_delete(bctx->m->root);
        }, LV_EVENT_CLICKED);
    }

    lv_obj_t* close_btn = createActionButton(root, "CLOSE", 516, 12, 140, 34, nullptr, nullptr);
    attachLvContext<ModalCtx>(close_btn, ModalCtx{&backend, root}, [](lv_event_t* e) {
        if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
        auto* ctx = static_cast<ModalCtx*>(lv_event_get_user_data(e));
        if (ctx && ctx->root) lv_obj_delete(ctx->root);
    }, LV_EVENT_CLICKED);
}

lv_obj_t* createFmAlgorithmSelector(lv_obj_t* parent, IUiBackend& backend,
                                    int32_t x, int32_t y, int32_t w, int32_t h) {
    if (!parent) return nullptr;

    lv_obj_t* card = lv_obj_create(parent);
    if (!card) return nullptr;
    applyPanelStyle(card, true);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);

    createLabel(card, "FM ALGORITHM", 8, 4, w - 16, &lv_font_montserrat_14, palette::kTextPrimary);

    auto* prev_btn = createActionButton(card, "<", 6, 26, 32, 28, nullptr, nullptr);
    auto* readout_btn = createActionButton(card, "ALGO 01 / 32", 42, 26, w - 84, 28, nullptr, nullptr);
    auto* next_btn = createActionButton(card, ">", w - 38, 26, 32, 28, nullptr, nullptr);

    lv_obj_t* readout_lbl = readout_btn ? lv_obj_get_child(readout_btn, 0) : nullptr;
    lv_obj_t* status_lbl = createLabel(card, "ORIGINAL: 01", 8, 56, w - 16, &lv_font_montserrat_14, palette::kTextSecondary);

    auto* ctx = attachLvContext<AlgoSelectorCtx>(
        card,
        AlgoSelectorCtx{&backend, card, readout_btn, readout_lbl, status_lbl, prev_btn, next_btn},
        nullptr);

    if (prev_btn) {
        attachLvContext<AlgoSelectorCtx>(prev_btn, *ctx, [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
            auto* c = static_cast<AlgoSelectorCtx*>(lv_event_get_user_data(e));
            if (!c || !c->backend) return;
            const int cur = c->backend->model().getInt("fm.algo", 1);
            if (cur > 1) c->backend->requestParameterInt("fm.algo", cur - 1);
            refreshSelector(c);
        }, LV_EVENT_CLICKED);
    }

    if (next_btn) {
        attachLvContext<AlgoSelectorCtx>(next_btn, *ctx, [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
            auto* c = static_cast<AlgoSelectorCtx*>(lv_event_get_user_data(e));
            if (!c || !c->backend) return;
            const int cur = c->backend->model().getInt("fm.algo", 1);
            if (cur < 32) c->backend->requestParameterInt("fm.algo", cur + 1);
            refreshSelector(c);
        }, LV_EVENT_CLICKED);
    }

    if (readout_btn) {
        attachLvContext<AlgoSelectorCtx>(readout_btn, *ctx, [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
            auto* c = static_cast<AlgoSelectorCtx*>(lv_event_get_user_data(e));
            if (!c || !c->backend) return;
            showFmAlgorithmPicker(c->card, *c->backend);
        }, LV_EVENT_CLICKED);
    }

    lv_obj_add_event_cb(card, [](lv_event_t* e) {
        auto* c = static_cast<AlgoSelectorCtx*>(lv_event_get_user_data(e));
        if (c) refreshSelector(c);
    }, LV_EVENT_REFRESH, ctx);

    refreshSelector(ctx);
    return card;
}

lv_obj_t* createFmTopologyDiagram(lv_obj_t* parent, IUiBackend& backend,
                                  int32_t x, int32_t y, int32_t w, int32_t h) {
    if (!parent) return nullptr;

    lv_obj_t* panel = lv_obj_create(parent);
    if (!panel) return nullptr;
    applyPanelStyle(panel, true);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, w, h);

    lv_obj_t* title_lbl = createLabel(panel, "TOPOLOGY: ALGO 01 / 32", 8, 4, w - 16, &lv_font_montserrat_14, palette::kTextPrimary);

    auto* ctx = attachLvContext<TopologyDiagramCtx>(panel, TopologyDiagramCtx{&backend, panel, title_lbl}, nullptr);

    // Place 6 operator tiles horizontally
    const int op_w = 88;
    const int op_h = 44;
    const int total_ops_w = 6 * op_w;
    const int avail_space = w - 16 - total_ops_w;
    const int gap = avail_space > 0 ? (avail_space / 5) : 6;
    const int start_x = 8;
    const int start_y = 24;

    struct OpBtnCtx {
        TopologyDiagramCtx* dctx;
        unsigned op;
    };

    for (unsigned op = 1; op <= 6; ++op) {
        const unsigned idx = op - 1;
        const int bx = start_x + idx * (op_w + gap);
        lv_obj_t* btn = createActionButton(panel, "", bx, start_y, op_w, op_h, nullptr, nullptr);
        ctx->op_btns[idx] = btn;

        char init_name[16];
        std::snprintf(init_name, sizeof(init_name), "OP %u", op);
        lv_obj_t* olbl = createLabel(btn, init_name, 4, 2, op_w - 8, &lv_font_montserrat_14, palette::kTextPrimary);
        lv_obj_set_style_text_align(olbl, LV_TEXT_ALIGN_CENTER, 0);
        ctx->op_lbls[idx] = olbl;

        lv_obj_t* rlbl = createLabel(btn, "CARRIER", 4, 22, op_w - 8, &lv_font_montserrat_14, palette::kActive);
        lv_obj_set_style_text_align(rlbl, LV_TEXT_ALIGN_CENTER, 0);
        ctx->op_roles[idx] = rlbl;

        attachLvContext<OpBtnCtx>(btn, OpBtnCtx{ctx, op}, [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
            auto* bctx = static_cast<OpBtnCtx*>(lv_event_get_user_data(e));
            if (!bctx || !bctx->dctx || !bctx->dctx->backend) return;
            bctx->dctx->backend->requestParameterInt("fm.selected_op", bctx->op);
            refreshTopology(bctx->dctx);
        }, LV_EVENT_CLICKED);
    }

    lv_obj_add_event_cb(panel, [](lv_event_t* e) {
        auto* c = static_cast<TopologyDiagramCtx*>(lv_event_get_user_data(e));
        if (c) refreshTopology(c);
    }, LV_EVENT_REFRESH, ctx);

    refreshTopology(ctx);
    return panel;
}

} // namespace smk::p4::ui
