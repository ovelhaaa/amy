#include "ui_widgets.h"
#include "fm_topology.h"
#include <cstdio>
#include <cstring>

namespace smk::p4::ui {

namespace {

lv_obj_t* s_active_algo_modal = nullptr;
lv_obj_t* s_active_topo_modal = nullptr;

void closeModalObj(lv_obj_t*& modal_ref, bool async) {
    if (!modal_ref) return;
    lv_obj_t* target = modal_ref;
    modal_ref = nullptr;
    if (async) {
        lv_obj_add_flag(target, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(target, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_delete_async(target);
    } else {
        lv_obj_delete(target);
    }
}

struct AlgoSelectorCtx {
    IUiBackend* backend = nullptr;
    lv_obj_t*   card = nullptr;
    lv_obj_t*   readout_btn = nullptr;
    lv_obj_t*   readout_lbl = nullptr;
    lv_obj_t*   status_lbl = nullptr;
    lv_obj_t*   prev_btn = nullptr;
    lv_obj_t*   next_btn = nullptr;
    bool        compact_readout = false;
};

void refreshSelector(AlgoSelectorCtx* ctx) {
    if (!ctx || !ctx->backend) return;
    const bool is_fm = (ctx->backend->model().activePatch().family == SynthFamily::FM);
    const int current_algo = ctx->backend->model().getInt("fm.algo", 1);
    const char* edit_status = ctx->backend->soundEditStatus();

    char abuf[32];
    if (ctx->compact_readout) {
        std::snprintf(abuf, sizeof(abuf), "A%02d/32", current_algo);
    } else {
        std::snprintf(abuf, sizeof(abuf), "ALGO %02d / 32", current_algo);
    }
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
    lv_obj_t*   expand_btn = nullptr;
    lv_obj_t*   out_btn = nullptr;
    lv_obj_t*   out_lbl = nullptr;
    lv_obj_t*   op_btns[6] = {};
    lv_obj_t*   op_lbls[6] = {};
    lv_obj_t*   op_roles[6] = {};
    lv_obj_t*   lines[20] = {};
    lv_point_precise_t line_points[20][4] = {};
    int32_t     diag_w = 460;
    int32_t     diag_h = 70;
    int32_t     diag_y_offset = 24;
    bool        expanded_labels = false;
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
        if (ctx->out_btn) lv_obj_add_state(ctx->out_btn, LV_STATE_DISABLED);
        for (unsigned e = 0; e < 20; ++e) {
            if (ctx->lines[e]) lv_obj_add_flag(ctx->lines[e], LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    const auto topo = smk::p4::fm::deriveTopology(current_algo);
    // Query the content box after styles/layout, including padding and borders.
    // The expanded view reserves its footer for legend/status.
    lv_obj_update_layout(ctx->panel);
    ctx->diag_w = lv_obj_get_content_width(ctx->panel);
    ctx->diag_h = lv_obj_get_content_height(ctx->panel) - ctx->diag_y_offset -
                  (ctx->expanded_labels ? 62 : 0);
    auto graph = smk::p4::fm::generateLayout(topo, int16_t(ctx->diag_w), int16_t(ctx->diag_h));
    if (!ctx->expanded_labels) {
        // Compact mode is a labelled operator summary, not a compressed graph.
        // Full musical routing remains available through EXPAND.
        for (auto& node : graph.nodes) {
            node.y = 10;
            node.h = 44;
        }
        graph.out_node.y = 10;
        graph.out_node.h = 44;
        graph.edge_count = 0;
    }

    char fbuf[128];
    smk::p4::fm::formatRoutingFormula(topo, fbuf, sizeof(fbuf));

    char tbuf[160];
    if (ctx->expanded_labels) {
        std::snprintf(tbuf, sizeof(tbuf), "ALGO %02d/32: %s", current_algo, fbuf);
    } else {
        std::snprintf(tbuf, sizeof(tbuf), "ALGO %02d | RESUMO: %u CARR / %u MOD | ROTAS EM EXPAND",
                      current_algo, topo.carrierCount(), topo.modulatorCount());
    }
    if (ctx->title_lbl) lv_label_set_text(ctx->title_lbl, tbuf);

    for (unsigned i = 0; i < 6; ++i) {
        const auto& node = graph.nodes[i];
        lv_obj_t* btn = ctx->op_btns[i];
        if (!btn) continue;
        lv_obj_clear_state(btn, LV_STATE_DISABLED);
        lv_obj_set_pos(btn, node.x, node.y + ctx->diag_y_offset);
        lv_obj_set_size(btn, node.w, node.h);

        const bool is_carrier = node.is_carrier;
        const bool is_selected = (selected_op == int(node.op));
        const bool has_fb = node.has_fb;

        const uint32_t color = is_carrier ? palette::kActive : palette::kAccentPrimary;
        lv_obj_set_style_border_color(btn, lv_color_hex(is_selected ? palette::kTextPrimary : color), 0);
        lv_obj_set_style_border_width(btn, is_selected ? 3 : 1, 0);
        lv_obj_update_layout(btn);
        const int32_t label_w = lv_obj_get_content_width(btn);

        char op_name[20];
        if (node.w >= 68) {
            std::snprintf(op_name, sizeof(op_name), "OP %u%s", node.op, has_fb ? " [FB]" : "");
        } else {
            std::snprintf(op_name, sizeof(op_name), "O%u%s", node.op, has_fb ? "[FB]" : "");
        }
        if (ctx->op_lbls[i]) {
            lv_obj_set_pos(ctx->op_lbls[i], 0, 0);
            lv_obj_set_size(ctx->op_lbls[i], label_w, 16);
            lv_label_set_text(ctx->op_lbls[i], op_name);
            lv_obj_set_style_text_color(ctx->op_lbls[i], lv_color_hex(is_selected ? palette::kTextPrimary : color), 0);
        }

        if (ctx->op_roles[i]) {
            const int32_t role_y = 17;
            lv_obj_set_pos(ctx->op_roles[i], 0, role_y);
            lv_obj_set_size(ctx->op_roles[i], label_w, 16);
            lv_label_set_text(ctx->op_roles[i], ctx->expanded_labels ? (is_carrier ? "CARRIER" : "MOD") : (is_carrier ? "CARR" : "MOD"));
            lv_obj_set_style_text_color(ctx->op_roles[i], lv_color_hex(color), 0);
        }
    }

    if (ctx->out_btn) {
        lv_obj_clear_state(ctx->out_btn, LV_STATE_DISABLED);
        lv_obj_set_pos(ctx->out_btn, graph.out_node.x, graph.out_node.y + ctx->diag_y_offset);
        lv_obj_set_size(ctx->out_btn, graph.out_node.w, graph.out_node.h);
        lv_obj_set_style_border_color(ctx->out_btn, lv_color_hex(palette::kActive), 0);
        lv_obj_set_style_border_width(ctx->out_btn, 1, 0);
        if (ctx->out_lbl) {
            lv_obj_set_size(ctx->out_lbl, graph.out_node.w - 4, 16);
            lv_obj_center(ctx->out_lbl);
        }
    }

    for (unsigned e = 0; e < graph.edge_count && e < 20; ++e) {
        const auto& edge = graph.edges[e];
        lv_obj_t* line = ctx->lines[e];
        if (!line) continue;
        for (uint8_t p = 0; p < edge.point_count; ++p) {
            ctx->line_points[e][p].x = edge.points[p].x;
            ctx->line_points[e][p].y = edge.points[p].y + ctx->diag_y_offset;
        }
        lv_line_set_points(line, ctx->line_points[e], edge.point_count);
        lv_obj_clear_flag(line, LV_OBJ_FLAG_HIDDEN);

        if (edge.type == smk::p4::fm::EdgeType::Modulation) {
            lv_obj_set_style_line_color(line, lv_color_hex(palette::kAccentPrimary), 0);
            lv_obj_set_style_line_width(line, 2, 0);
        } else if (edge.type == smk::p4::fm::EdgeType::AudioOut) {
            lv_obj_set_style_line_color(line, lv_color_hex(palette::kActive), 0);
            lv_obj_set_style_line_width(line, 2, 0);
        } else if (edge.type == smk::p4::fm::EdgeType::Feedback) {
            lv_obj_set_style_line_color(line, lv_color_hex(palette::kWarning), 0);
            lv_obj_set_style_line_width(line, 2, 0);
        }
    }
    for (unsigned e = graph.edge_count; e < 20; ++e) {
        if (ctx->lines[e]) lv_obj_add_flag(ctx->lines[e], LV_OBJ_FLAG_HIDDEN);
    }
}

} // namespace

void closeFmModals(bool async) {
    closeModalObj(s_active_algo_modal, async);
    closeModalObj(s_active_topo_modal, async);
}

void refreshFmModals() {
    if (s_active_algo_modal) lv_obj_send_event(s_active_algo_modal, LV_EVENT_REFRESH, nullptr);
    if (s_active_topo_modal) lv_obj_send_event(s_active_topo_modal, LV_EVENT_REFRESH, nullptr);
}

void showFmAlgorithmPicker(lv_obj_t* anchor, IUiBackend& backend) {
    lv_obj_t* screen = anchor ? lv_obj_get_screen(anchor) : lv_screen_active();
    if (!screen) screen = lv_screen_active();
    if (!screen) return;

    if (s_active_algo_modal) {
        lv_obj_move_foreground(s_active_algo_modal);
        return;
    }

    lv_obj_t* root = lv_obj_create(screen);
    if (!root) return;
    applyPanelStyle(root, true);
    lv_obj_set_pos(root, 60, 48);
    lv_obj_set_size(root, 680, 380);
    s_active_algo_modal = root;
    lv_obj_add_event_cb(root, [](lv_event_t* e) {
        if (lv_event_get_target(e) == s_active_algo_modal) s_active_algo_modal = nullptr;
    }, LV_EVENT_DELETE, nullptr);

    createLabel(root, "SELECT FM ALGORITHM (1 - 32)", 10, 4, 480, &lv_font_montserrat_14, palette::kAccentPrimary);
    createLabel(root, "Selecione 1-32; solte as notas antes de aplicar.", 10, 22, 480, &lv_font_montserrat_14, palette::kTextSecondary);

    const int current_algo = backend.model().getInt("fm.algo", 1);
    lv_obj_t* status_lbl = createLabel(root, "STATUS: READY", 10, 40, 480, &lv_font_montserrat_14, palette::kTextSecondary);

    struct ModalCtx {
        IUiBackend* backend = nullptr;
        lv_obj_t* root = nullptr;
        lv_obj_t* status_lbl = nullptr;
        lv_obj_t* btn_objs[32] = {};
        int pending_algo = 0;
        uint32_t pending_token = 0;
        uint16_t initial_patch_id = 0;
        int confirmed_algo = 1;
    };
    const uint16_t cur_patch_id = backend.model().activePatch().id;
    auto* mctx = attachLvContext<ModalCtx>(root, ModalCtx{&backend, root, status_lbl, {}, 0, 0, cur_patch_id, current_algo}, nullptr);
    if (!mctx) { closeModalObj(s_active_algo_modal, true); return; }

    // 4 rows of 8 buttons = 32 algorithms
    const int start_x = 10, start_y = 62;
    const int btn_w = 74, btn_h = 62;
    const int gap_x = 8, gap_y = 6;

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
        mctx->btn_objs[i - 1] = btn;
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
            auto* m = bctx->m;

            if (m->pending_algo != 0) {
                if (m->status_lbl) {
                    lv_label_set_text(m->status_lbl, "ALGORITHM CHANGE PENDING");
                    lv_obj_set_style_text_color(m->status_lbl, lv_color_hex(palette::kWarning), 0);
                }
                return;
            }
            if (bctx->algo == m->confirmed_algo && m->pending_algo == 0) {
                if (m->status_lbl) {
                    char abuf[48];
                    std::snprintf(abuf, sizeof(abuf), "ALGORITHM %02d APPLIED", bctx->algo);
                    lv_label_set_text(m->status_lbl, abuf);
                    lv_obj_set_style_text_color(m->status_lbl, lv_color_hex(palette::kActive), 0);
                }
                return;
            }

            const auto res = m->backend->requestParameterInt("fm.algo", bctx->algo);
            if (res.state == UiCommandState::Rejected) {
                m->pending_algo = 0;
                if (m->status_lbl) {
                    lv_label_set_text(m->status_lbl, res.notice ? res.notice : "RELEASE NOTES TO CHANGE ALGORITHM");
                    lv_obj_set_style_text_color(m->status_lbl, lv_color_hex(palette::kWarning), 0);
                }
                return;
            }

            m->pending_algo = bctx->algo;
            m->pending_token = res.token;
            m->initial_patch_id = m->backend->model().activePatch().id;
            if (res.state == UiCommandState::Confirmed || res.state == UiCommandState::DemoApplied) {
                m->confirmed_algo = bctx->algo;
                m->pending_algo = 0;
                if (m->status_lbl) {
                    char abuf[48];
                    std::snprintf(abuf, sizeof(abuf), "ALGORITHM %02d APPLIED", bctx->algo);
                    lv_label_set_text(m->status_lbl, abuf);
                    lv_obj_set_style_text_color(m->status_lbl, lv_color_hex(palette::kActive), 0);
                }
            } else {
                if (m->status_lbl) {
                    char pbuf[48];
                    std::snprintf(pbuf, sizeof(pbuf), "ALGORITHM %02d REQUESTED; WAIT OWNER", bctx->algo);
                    lv_label_set_text(m->status_lbl, pbuf);
                    lv_obj_set_style_text_color(m->status_lbl, lv_color_hex(palette::kAccentPrimary), 0);
                }
            }
        }, LV_EVENT_CLICKED);
    }

    lv_obj_t* close_btn = createActionButton(root, "CLOSE", 516, 8, 140, 34, nullptr, nullptr);
    attachLvContext<ModalCtx*>(close_btn, mctx, [](lv_event_t* e) {
        if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
        closeModalObj(s_active_algo_modal, true);
    }, LV_EVENT_CLICKED);

    lv_obj_add_event_cb(root, [](lv_event_t* e) {
        auto* m = static_cast<ModalCtx*>(lv_event_get_user_data(e));
        if (!m || !m->backend) return;
        const auto active_patch = m->backend->model().activePatch();
        if (m->pending_algo != 0) {
            if (active_patch.id != m->initial_patch_id) {
                m->pending_algo = 0;
                m->confirmed_algo = m->backend->model().getInt("fm.algo", 1);
                if (m->status_lbl) {
                    lv_label_set_text(m->status_lbl, "PATCH CHANGED; ALGORITHM CANCELED");
                    lv_obj_set_style_text_color(m->status_lbl, lv_color_hex(palette::kWarning), 0);
                }
            } else {
                const auto ack = m->backend->fmAlgorithmResult();
                if (ack.token == m->pending_token && ack.state == UiCommandState::Confirmed) {
                    m->confirmed_algo = m->pending_algo;
                    m->pending_algo = 0;
                    if (m->status_lbl) {
                        char abuf[48];
                        std::snprintf(abuf, sizeof(abuf), "ALGORITHM %02d APPLIED", m->confirmed_algo);
                        lv_label_set_text(m->status_lbl, abuf);
                        lv_obj_set_style_text_color(m->status_lbl, lv_color_hex(palette::kActive), 0);
                    }
                } else if (ack.token == m->pending_token && ack.state == UiCommandState::Rejected) {
                    m->pending_algo = 0;
                    if (m->status_lbl) {
                        lv_label_set_text(m->status_lbl, ack.notice ? ack.notice : "ALGORITHM CHANGE NOT CONFIRMED");
                        lv_obj_set_style_text_color(m->status_lbl, lv_color_hex(palette::kWarning), 0);
                    }
                }
            }
        }
        for (int i = 1; i <= 32; ++i) {
            lv_obj_t* btn = m->btn_objs[i - 1];
            if (!btn) continue;
            char text[16];
            std::snprintf(text, sizeof(text), "%02d%s", i,
                          i == m->pending_algo ? "\nWAIT" : (i == m->confirmed_algo ? "\nACT" : ""));
            lv_label_set_text(lv_obj_get_child(btn, 0), text);
            if (i == m->confirmed_algo) {
                lv_obj_set_style_border_color(btn, lv_color_hex(palette::kActive), 0);
                lv_obj_set_style_border_width(btn, 2, 0);
            } else {
                lv_obj_set_style_border_color(btn, lv_color_hex(palette::kBorder), 0);
                lv_obj_set_style_border_width(btn, 1, 0);
            }
        }
    }, LV_EVENT_REFRESH, mctx);
}

void showFmTopologyModal(lv_obj_t* anchor, IUiBackend& backend) {
    lv_obj_t* screen = anchor ? lv_obj_get_screen(anchor) : lv_screen_active();
    if (!screen) screen = lv_screen_active();
    if (!screen) return;

    if (s_active_topo_modal) {
        lv_obj_move_foreground(s_active_topo_modal);
        return;
    }

    constexpr int32_t kModalW = 720;
    constexpr int32_t kModalH = 380;
    constexpr int32_t kInnerW = kModalW - 14;

    lv_obj_t* root = lv_obj_create(screen);
    if (!root) return;
    applyPanelStyle(root, true);
    lv_obj_set_pos(root, 40, 48);
    lv_obj_set_size(root, kModalW, kModalH);
    s_active_topo_modal = root;
    lv_obj_add_event_cb(root, [](lv_event_t* e) {
        if (lv_event_get_target(e) == s_active_topo_modal) s_active_topo_modal = nullptr;
    }, LV_EVENT_DELETE, nullptr);

    lv_obj_t* header_lbl = createLabel(root, "FM TOPOLOGY: ALGORITHM 01 / 32", 4, 2, kInnerW - 154, &lv_font_montserrat_14, palette::kAccentPrimary);
    lv_obj_t* formula_lbl = createLabel(root, "", 4, 22, kInnerW - 154, &lv_font_montserrat_14, palette::kTextSecondary);
    if (formula_lbl) lv_label_set_long_mode(formula_lbl, LV_LABEL_LONG_MODE_DOTS);

    lv_obj_t* close_btn = createActionButton(root, "CLOSE", kInnerW - 142, 2, 140, 34, nullptr, nullptr);
    lv_obj_add_event_cb(close_btn, [](lv_event_t* e) {
        if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
        closeModalObj(s_active_topo_modal, true);
    }, LV_EVENT_CLICKED, nullptr);

    struct ExpandedModalCtx {
        TopologyDiagramCtx diag{};
        lv_obj_t* header_lbl = nullptr;
        lv_obj_t* status_lbl = nullptr;
    };

    lv_obj_t* legend_lbl = createLabel(
        root,
        "CARR: VERDE -> OUT | MOD: AMBAR | FB: LARANJA | SELECAO: BRANCO",
        4, 314, kInnerW - 8, &lv_font_montserrat_14, palette::kTextSecondary);
    if (legend_lbl) lv_label_set_long_mode(legend_lbl, LV_LABEL_LONG_MODE_DOTS);

    lv_obj_t* status_lbl = createLabel(root, "", 4, 338, kInnerW - 8, &lv_font_montserrat_14, palette::kActive);
    if (status_lbl) lv_label_set_long_mode(status_lbl, LV_LABEL_LONG_MODE_DOTS);

    ExpandedModalCtx init_ctx{};
    init_ctx.diag.backend = &backend;
    init_ctx.diag.panel = root;
    init_ctx.diag.title_lbl = formula_lbl;
    init_ctx.diag.diag_w = kInnerW;
    init_ctx.diag.diag_h = 258;
    init_ctx.diag.diag_y_offset = 46;
    init_ctx.diag.expanded_labels = true;
    init_ctx.header_lbl = header_lbl;
    init_ctx.status_lbl = status_lbl;

    auto* mctx = attachLvContext<ExpandedModalCtx>(root, init_ctx, nullptr);
    if (!mctx) { closeModalObj(s_active_topo_modal, true); return; }

    for (unsigned i = 0; i < 20; ++i) {
        lv_obj_t* line = lv_line_create(root);
        mctx->diag.lines[i] = line;
        lv_obj_add_flag(line, LV_OBJ_FLAG_HIDDEN);
    }

    mctx->diag.out_btn = createActionButton(root, "", kInnerW - 60, 140, 52, 44, nullptr, nullptr);
    mctx->diag.out_lbl = createLabel(mctx->diag.out_btn, "OUT", 2, 12, 48, &lv_font_montserrat_14, palette::kActive);
    lv_obj_set_style_text_align(mctx->diag.out_lbl, LV_TEXT_ALIGN_CENTER, 0);

    struct BigOpCtx {
        ExpandedModalCtx* m;
        unsigned op;
    };

    for (unsigned i = 0; i < 6; ++i) {
        const unsigned op = 6 - i;
        lv_obj_t* btn = createActionButton(root, "", 8 + i * 96, 60, 88, 46, nullptr, nullptr);
        lv_obj_set_style_pad_all(btn, 2, 0);
        mctx->diag.op_btns[i] = btn;

        lv_obj_t* olbl = createLabel(btn, "OP", 2, 2, 82, &lv_font_montserrat_14, palette::kTextPrimary);
        lv_obj_set_style_text_align(olbl, LV_TEXT_ALIGN_CENTER, 0);
        mctx->diag.op_lbls[i] = olbl;

        lv_obj_t* rlbl = createLabel(btn, "CARR", 2, 18, 82, &lv_font_montserrat_14, palette::kActive);
        lv_obj_set_style_text_align(rlbl, LV_TEXT_ALIGN_CENTER, 0);
        mctx->diag.op_roles[i] = rlbl;

        attachLvContext<BigOpCtx>(btn, BigOpCtx{mctx, op}, [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
            auto* bctx = static_cast<BigOpCtx*>(lv_event_get_user_data(e));
            if (!bctx || !bctx->m || !bctx->m->diag.backend) return;
            const auto res = bctx->m->diag.backend->requestParameterInt("fm.selected_op", bctx->op);
            refreshTopology(&bctx->m->diag);
            if (bctx->m->status_lbl) {
                if (res.state == UiCommandState::Rejected) {
                    lv_label_set_text(bctx->m->status_lbl, res.notice ? res.notice : "OPERATOR CHANGE REJECTED");
                    lv_obj_set_style_text_color(bctx->m->status_lbl, lv_color_hex(palette::kWarning), 0);
                } else if (res.state == UiCommandState::Requested) {
                    lv_label_set_text(bctx->m->status_lbl, "OPERATOR REQUESTED; WAIT OWNER");
                    lv_obj_set_style_text_color(bctx->m->status_lbl, lv_color_hex(palette::kWarning), 0);
                } else {
                    const int algo = bctx->m->diag.backend->model().getInt("fm.algo", 1);
                    const auto t = smk::p4::fm::deriveTopology(algo);
                    char sbuf[80];
                    std::snprintf(sbuf, sizeof(sbuf), "SELECTED: OP %u (%s%s)", bctx->op,
                                  t.isCarrier(bctx->op) ? "CARRIER -> AUDIO OUT" : "MODULATOR",
                                  t.hasFeedback(bctx->op) ? " | FEEDBACK" : "");
                    lv_label_set_text(bctx->m->status_lbl, sbuf);
                    lv_obj_set_style_text_color(bctx->m->status_lbl, lv_color_hex(palette::kActive), 0);
                }
            }
        }, LV_EVENT_CLICKED);
    }

    auto sync_modal = [](ExpandedModalCtx* m) {
        if (!m || !m->diag.backend) return;
        const int algo = m->diag.backend->model().getInt("fm.algo", 1);
        const int sel = m->diag.backend->model().getInt("fm.selected_op", 1);
        if (m->header_lbl) {
            char hbuf[64];
            std::snprintf(hbuf, sizeof(hbuf), "FM TOPOLOGY: ALGORITHM %02d / 32", algo);
            lv_label_set_text(m->header_lbl, hbuf);
        }
        refreshTopology(&m->diag);
        if (m->status_lbl) {
            const auto t = smk::p4::fm::deriveTopology(algo);
            char sbuf[80];
            std::snprintf(sbuf, sizeof(sbuf), "SELECTED: OP %d (%s%s)", sel,
                          t.isCarrier(unsigned(sel)) ? "CARRIER -> AUDIO OUT" : "MODULATOR",
                          t.hasFeedback(unsigned(sel)) ? " | FEEDBACK" : "");
            lv_label_set_text(m->status_lbl, sbuf);
            lv_obj_set_style_text_color(m->status_lbl, lv_color_hex(palette::kActive), 0);
        }
    };

    lv_obj_add_event_cb(root, [](lv_event_t* e) {
        auto* m = static_cast<ExpandedModalCtx*>(lv_event_get_user_data(e));
        if (!m || !m->diag.backend) return;
        const int algo = m->diag.backend->model().getInt("fm.algo", 1);
        const int sel = m->diag.backend->model().getInt("fm.selected_op", 1);
        if (m->header_lbl) {
            char hbuf[64];
            std::snprintf(hbuf, sizeof(hbuf), "FM TOPOLOGY: ALGORITHM %02d / 32", algo);
            lv_label_set_text(m->header_lbl, hbuf);
        }
        refreshTopology(&m->diag);
        if (m->status_lbl) {
            const auto t = smk::p4::fm::deriveTopology(algo);
            char sbuf[80];
            std::snprintf(sbuf, sizeof(sbuf), "SELECTED: OP %d (%s%s)", sel,
                          t.isCarrier(unsigned(sel)) ? "CARRIER -> AUDIO OUT" : "MODULATOR",
                          t.hasFeedback(unsigned(sel)) ? " | FEEDBACK" : "");
            lv_label_set_text(m->status_lbl, sbuf);
        }
    }, LV_EVENT_REFRESH, mctx);

    sync_modal(mctx);
}

lv_obj_t* createFmAlgorithmSelector(lv_obj_t* parent, IUiBackend& backend,
                                    int32_t x, int32_t y, int32_t w, int32_t h) {
    if (!parent) return nullptr;

    lv_obj_t* card = lv_obj_create(parent);
    if (!card) return nullptr;
    applyPanelStyle(card, true);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);

    constexpr int32_t kPanelInset = 14;
    const int32_t inner_w = (w > kPanelInset) ? (w - kPanelInset) : w;
    const int32_t inner_h = (h > kPanelInset) ? (h - kPanelInset) : h;
    const bool compact_h = (h < 76);

    lv_obj_t* status_lbl = nullptr;
    lv_obj_t* prev_btn = nullptr;
    lv_obj_t* readout_btn = nullptr;
    lv_obj_t* next_btn = nullptr;

    if (compact_h) {
        createLabel(card, "FM ALGORITHM", 0, 0, inner_w - 72, &lv_font_montserrat_14, palette::kTextPrimary);
        status_lbl = createLabel(card, "ACTIVE", inner_w - 70, 0, 70, &lv_font_montserrat_14, palette::kTextSecondary);
        if (status_lbl) {
            lv_obj_set_style_text_align(status_lbl, LV_TEXT_ALIGN_RIGHT, 0);
            lv_label_set_long_mode(status_lbl, LV_LABEL_LONG_MODE_DOTS);
        }
        const int32_t btn_y = inner_h - 28;
        const int32_t step_w = 32;
        const int32_t read_w = inner_w - step_w * 2 - 8;
        prev_btn = createActionButton(card, "<", 0, btn_y, step_w, 28, nullptr, nullptr);
        readout_btn = createActionButton(card, "ALGO 01 / 32", step_w + 4, btn_y, read_w, 28, nullptr, nullptr);
        next_btn = createActionButton(card, ">", inner_w - step_w, btn_y, step_w, 28, nullptr, nullptr);
    } else {
        createLabel(card, "FM ALGORITHM", 2, 0, inner_w - 4, &lv_font_montserrat_14, palette::kTextPrimary);
        const int32_t step_w = 32;
        const int32_t read_w = inner_w - step_w * 2 - 8;
        prev_btn = createActionButton(card, "<", 0, 20, step_w, 28, nullptr, nullptr);
        readout_btn = createActionButton(card, "ALGO 01 / 32", step_w + 4, 20, read_w, 28, nullptr, nullptr);
        next_btn = createActionButton(card, ">", inner_w - step_w, 20, step_w, 28, nullptr, nullptr);
        status_lbl = createLabel(card, "ORIGINAL: 01", 2, 52, inner_w - 4, &lv_font_montserrat_14, palette::kTextSecondary);
        if (status_lbl) lv_label_set_long_mode(status_lbl, LV_LABEL_LONG_MODE_DOTS);
    }

    const int32_t read_w = readout_btn ? lv_obj_get_width(readout_btn) : 60;
    const bool compact_readout = (read_w < 96);

    lv_obj_t* readout_lbl = readout_btn ? lv_obj_get_child(readout_btn, 0) : nullptr;

    auto* ctx = attachLvContext<AlgoSelectorCtx>(
        card,
        AlgoSelectorCtx{&backend, card, readout_btn, readout_lbl, status_lbl, prev_btn, next_btn, compact_readout},
        nullptr);

    if (prev_btn) {
        attachLvContext<AlgoSelectorCtx>(prev_btn, *ctx, [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
            auto* c = static_cast<AlgoSelectorCtx*>(lv_event_get_user_data(e));
            if (!c || !c->backend) return;
            const int cur = c->backend->model().getInt("fm.algo", 1);
            if (cur > 1) {
                const auto res = c->backend->requestParameterInt("fm.algo", cur - 1);
                if (res.state == UiCommandState::Rejected && c->status_lbl) {
                    lv_label_set_text(c->status_lbl, res.notice ? res.notice : "REJECTED");
                    lv_obj_set_style_text_color(c->status_lbl, lv_color_hex(palette::kWarning), 0);
                }
            }
            refreshSelector(c);
        }, LV_EVENT_CLICKED);
    }

    if (next_btn) {
        attachLvContext<AlgoSelectorCtx>(next_btn, *ctx, [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
            auto* c = static_cast<AlgoSelectorCtx*>(lv_event_get_user_data(e));
            if (!c || !c->backend) return;
            const int cur = c->backend->model().getInt("fm.algo", 1);
            if (cur < 32) {
                const auto res = c->backend->requestParameterInt("fm.algo", cur + 1);
                if (res.state == UiCommandState::Rejected && c->status_lbl) {
                    lv_label_set_text(c->status_lbl, res.notice ? res.notice : "REJECTED");
                    lv_obj_set_style_text_color(c->status_lbl, lv_color_hex(palette::kWarning), 0);
                }
            }
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

    constexpr int32_t kPanelInset = 14;
    const int32_t inner_w = (w > kPanelInset) ? (w - kPanelInset) : w;
    const int32_t inner_h = (h > kPanelInset) ? (h - kPanelInset) : h;
    constexpr int32_t kHeaderH = 22;

    lv_obj_t* title_lbl = createLabel(panel, "TOPOLOGY: ALGO 01 / 32", 0, 1, inner_w - 82, &lv_font_montserrat_14, palette::kTextPrimary);
    if (title_lbl) lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_t* expand_btn = createActionButton(panel, "EXPAND", inner_w - 76, 0, 76, 20, nullptr, nullptr);

    TopologyDiagramCtx init_ctx{};
    init_ctx.backend = &backend;
    init_ctx.panel = panel;
    init_ctx.title_lbl = title_lbl;
    init_ctx.expand_btn = expand_btn;
    init_ctx.diag_w = inner_w;
    init_ctx.diag_h = (inner_h > kHeaderH) ? (inner_h - kHeaderH) : 56;
    init_ctx.diag_y_offset = kHeaderH;
    init_ctx.expanded_labels = false;

    auto* ctx = attachLvContext<TopologyDiagramCtx>(panel, init_ctx, nullptr);

    for (unsigned i = 0; i < 20; ++i) {
        lv_obj_t* line = lv_line_create(panel);
        ctx->lines[i] = line;
        lv_obj_add_flag(line, LV_OBJ_FLAG_HIDDEN);
    }

    ctx->out_btn = createActionButton(panel, "", inner_w - 48, kHeaderH + 14, 44, 34, nullptr, nullptr);
    ctx->out_lbl = createLabel(ctx->out_btn, "OUT", 1, 8, 40, &lv_font_montserrat_14, palette::kActive);
    lv_obj_set_style_text_align(ctx->out_lbl, LV_TEXT_ALIGN_CENTER, 0);

    struct OpBtnCtx {
        TopologyDiagramCtx* dctx;
        unsigned op;
    };

    for (unsigned i = 0; i < 6; ++i) {
        const unsigned op = 6 - i;
        lv_obj_t* btn = createActionButton(panel, "", 4 + i * 64, kHeaderH + 8, 54, 28, nullptr, nullptr);
        lv_obj_set_style_pad_all(btn, 1, 0);
        ctx->op_btns[i] = btn;

        char init_name[16];
        std::snprintf(init_name, sizeof(init_name), "OP %u", op);
        lv_obj_t* olbl = createLabel(btn, init_name, 1, 0, 50, &lv_font_montserrat_14, palette::kTextPrimary);
        lv_obj_set_style_text_align(olbl, LV_TEXT_ALIGN_CENTER, 0);
        ctx->op_lbls[i] = olbl;

        lv_obj_t* rlbl = createLabel(btn, "CARR", 1, 13, 50, &lv_font_montserrat_14, palette::kActive);
        lv_obj_set_style_text_align(rlbl, LV_TEXT_ALIGN_CENTER, 0);
        ctx->op_roles[i] = rlbl;

        attachLvContext<OpBtnCtx>(btn, OpBtnCtx{ctx, op}, [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
            auto* bctx = static_cast<OpBtnCtx*>(lv_event_get_user_data(e));
            if (!bctx || !bctx->dctx || !bctx->dctx->backend) return;
            bctx->dctx->backend->requestParameterInt("fm.selected_op", bctx->op);
            refreshTopology(bctx->dctx);
        }, LV_EVENT_CLICKED);
    }

    if (expand_btn) {
        attachLvContext<TopologyDiagramCtx>(expand_btn, *ctx, [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
            auto* c = static_cast<TopologyDiagramCtx*>(lv_event_get_user_data(e));
            if (!c || !c->backend) return;
            showFmTopologyModal(c->panel, *c->backend);
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
