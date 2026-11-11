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
    lv_obj_t*   expand_btn = nullptr;
    lv_obj_t*   out_btn = nullptr;
    lv_obj_t*   out_lbl = nullptr;
    lv_obj_t*   op_btns[6] = {};
    lv_obj_t*   op_lbls[6] = {};
    lv_obj_t*   op_roles[6] = {};
    lv_obj_t*   lines[20] = {};
    lv_point_precise_t line_points[20][4] = {};
};

bool isEditPending(IUiBackend* b) {
    if (!b) return false;
    const char* s = b->soundEditStatus();
    return s && std::strstr(s, "WAIT OWNER") != nullptr;
}

bool isEditRejected(IUiBackend* b) {
    if (!b) return false;
    const char* s = b->soundEditStatus();
    return s && std::strstr(s, "REJECTED") != nullptr;
}

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
    const auto graph = smk::p4::fm::generateLayout(topo, 576, 78);

    char fbuf[128];
    smk::p4::fm::formatRoutingFormula(topo, fbuf, sizeof(fbuf));

    char tbuf[160];
    std::snprintf(tbuf, sizeof(tbuf), "ALGO %02d/32: %s", current_algo, fbuf);
    if (ctx->title_lbl) lv_label_set_text(ctx->title_lbl, tbuf);

    // Update 6 operator nodes
    for (unsigned i = 0; i < 6; ++i) {
        const auto& node = graph.nodes[i];
        lv_obj_t* btn = ctx->op_btns[i];
        if (!btn) continue;
        lv_obj_clear_state(btn, LV_STATE_DISABLED);
        lv_obj_set_pos(btn, node.x, node.y);
        lv_obj_set_size(btn, node.w, node.h);

        const bool is_carrier = node.is_carrier;
        const bool is_selected = (selected_op == int(node.op));
        const bool has_fb = node.has_fb;

        const uint32_t color = is_carrier ? palette::kActive : palette::kAccentPrimary;
        lv_obj_set_style_border_color(btn, lv_color_hex(is_selected ? palette::kTextPrimary : color), 0);
        lv_obj_set_style_border_width(btn, is_selected ? 3 : 1, 0);

        char op_name[20];
        std::snprintf(op_name, sizeof(op_name), "OP %u%s", node.op, has_fb ? " [FB]" : "");
        if (ctx->op_lbls[i]) {
            lv_label_set_text(ctx->op_lbls[i], op_name);
            lv_obj_set_style_text_color(ctx->op_lbls[i], lv_color_hex(is_selected ? palette::kTextPrimary : color), 0);
        }

        if (ctx->op_roles[i]) {
            lv_label_set_text(ctx->op_roles[i], is_carrier ? "CARR" : "MOD");
            lv_obj_set_style_text_color(ctx->op_roles[i], lv_color_hex(color), 0);
        }
    }

    // Update OUT node
    if (ctx->out_btn) {
        lv_obj_clear_state(ctx->out_btn, LV_STATE_DISABLED);
        lv_obj_set_pos(ctx->out_btn, graph.out_node.x, graph.out_node.y);
        lv_obj_set_size(ctx->out_btn, graph.out_node.w, graph.out_node.h);
        lv_obj_set_style_border_color(ctx->out_btn, lv_color_hex(palette::kActive), 0);
        lv_obj_set_style_border_width(ctx->out_btn, 1, 0);
    }

    // Update lines
    for (unsigned e = 0; e < graph.edge_count && e < 20; ++e) {
        const auto& edge = graph.edges[e];
        lv_obj_t* line = ctx->lines[e];
        if (!line) continue;
        for (uint8_t p = 0; p < edge.point_count; ++p) {
            ctx->line_points[e][p].x = edge.points[p].x;
            ctx->line_points[e][p].y = edge.points[p].y;
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
    createLabel(root, "Touch an algorithm number to modify topology across all 8 voices", 16, 26, 480, &lv_font_montserrat_14, palette::kTextSecondary);

    const int current_algo = backend.model().getInt("fm.algo", 1);

    lv_obj_t* status_lbl = createLabel(root, "STATUS: READY", 16, 44, 480, &lv_font_montserrat_14, palette::kTextSecondary);

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

    // 4 rows of 8 buttons = 32 algorithms
    const int start_x = 16, start_y = 66;
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

            if (m->pending_algo != 0 && (isEditPending(m->backend) || m->backend->isBusy())) {
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
                // Immediate rejection: Do not close modal, do not change confirmed algorithm
                m->pending_algo = 0;
                if (m->status_lbl) {
                    lv_label_set_text(m->status_lbl, res.notice ? res.notice : "RELEASE NOTES TO CHANGE ALGORITHM");
                    lv_obj_set_style_text_color(m->status_lbl, lv_color_hex(palette::kWarning), 0);
                }
                return;
            }

            // Command accepted or requested
            m->pending_algo = bctx->algo;
            m->pending_token = res.token;
            m->initial_patch_id = m->backend->model().activePatch().id;
            if (res.state == UiCommandState::Confirmed) {
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

    lv_obj_t* close_btn = createActionButton(root, "CLOSE", 516, 12, 140, 34, nullptr, nullptr);
    attachLvContext<ModalCtx>(close_btn, ModalCtx{&backend, root, status_lbl, {}, 0, 0, cur_patch_id, current_algo}, [](lv_event_t* e) {
        if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
        auto* ctx = static_cast<ModalCtx*>(lv_event_get_user_data(e));
        if (ctx && ctx->root) lv_obj_delete(ctx->root);
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
            } else if (std::strcmp(m->backend->lastCommandParameterId(), "fm.algo") == 0) {
                if (m->backend->lastCommandState() == UiCommandState::Confirmed) {
                    const int cur = m->backend->model().getInt("fm.algo", m->confirmed_algo);
                    if (cur == m->pending_algo) {
                        m->confirmed_algo = cur;
                        m->pending_algo = 0;
                        if (m->status_lbl) {
                            char abuf[48];
                            std::snprintf(abuf, sizeof(abuf), "ALGORITHM %02d APPLIED", cur);
                            lv_label_set_text(m->status_lbl, abuf);
                            lv_obj_set_style_text_color(m->status_lbl, lv_color_hex(palette::kActive), 0);
                        }
                    }
                } else if (m->backend->lastCommandState() == UiCommandState::Rejected) {
                    m->pending_algo = 0;
                    if (m->status_lbl) {
                        const char* notice = m->backend->soundEditStatus();
                        if (notice && std::strstr(notice, "NOT CONFIRMED") != nullptr) {
                            lv_label_set_text(m->status_lbl, "ALGORITHM CHANGE NOT CONFIRMED");
                        } else {
                            lv_label_set_text(m->status_lbl, (notice && notice[0]) ? notice : "ALGORITHM REJECTED: OWNER ERROR");
                        }
                        lv_obj_set_style_text_color(m->status_lbl, lv_color_hex(palette::kWarning), 0);
                    }
                }
            } else if (!isEditPending(m->backend)) {
                const int cur = m->backend->model().getInt("fm.algo", m->confirmed_algo);
                if (cur == m->pending_algo) {
                    m->confirmed_algo = cur;
                    m->pending_algo = 0;
                    if (m->status_lbl) {
                        char abuf[48];
                        std::snprintf(abuf, sizeof(abuf), "ALGORITHM %02d APPLIED", cur);
                        lv_label_set_text(m->status_lbl, abuf);
                        lv_obj_set_style_text_color(m->status_lbl, lv_color_hex(palette::kActive), 0);
                    }
                } else if (isEditRejected(m->backend)) {
                    m->pending_algo = 0;
                    if (m->status_lbl) {
                        const char* notice = m->backend->soundEditStatus();
                        lv_label_set_text(m->status_lbl, (notice && notice[0]) ? notice : "ALGORITHM REJECTED: OWNER ERROR");
                        lv_obj_set_style_text_color(m->status_lbl, lv_color_hex(palette::kWarning), 0);
                    }
                }
            }
        }
        for (int i = 1; i <= 32; ++i) {
            lv_obj_t* btn = m->btn_objs[i - 1];
            if (!btn) continue;
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

    lv_obj_t* root = lv_obj_create(screen);
    if (!root) return;
    applyPanelStyle(root, true);
    lv_obj_set_pos(root, 40, 40);
    lv_obj_set_size(root, 720, 396);

    const int current_algo = backend.model().getInt("fm.algo", 1);
    const auto topo = smk::p4::fm::deriveTopology(current_algo);

    char title_buf[64];
    std::snprintf(title_buf, sizeof(title_buf), "FM TOPOLOGY: ALGORITHM %02d / 32", current_algo);
    createLabel(root, title_buf, 16, 8, 480, &lv_font_montserrat_14, palette::kAccentPrimary);

    char fbuf[128];
    smk::p4::fm::formatRoutingFormula(topo, fbuf, sizeof(fbuf));
    createLabel(root, fbuf, 16, 28, 520, &lv_font_montserrat_14, palette::kTextSecondary);

    struct BigModalCtx {
        IUiBackend* backend;
        lv_obj_t* root;
        lv_obj_t* op_btns[6];
    };
    auto* mctx = attachLvContext<BigModalCtx>(root, BigModalCtx{&backend, root, {}}, nullptr);

    const int selected_op = backend.model().getInt("fm.selected_op", 1);
    const int start_x = 24, start_y = 64;
    const int card_w = 100, card_h = 130;
    const int gap_x = 12;

    struct BigOpCtx {
        BigModalCtx* m;
        unsigned op;
    };

    for (unsigned i = 0; i < 6; ++i) {
        const unsigned op = 6 - i;
        const int bx = start_x + i * (card_w + gap_x);
        lv_obj_t* btn = createActionButton(root, "", bx, start_y, card_w, card_h, nullptr, nullptr);
        mctx->op_btns[i] = btn;

        const bool is_carrier = topo.isCarrier(op);
        const bool is_selected = (selected_op == int(op));
        const bool has_fb = topo.hasFeedback(op);
        const uint32_t color = is_carrier ? palette::kActive : palette::kAccentPrimary;

        lv_obj_set_style_border_color(btn, lv_color_hex(is_selected ? palette::kTextPrimary : color), 0);
        lv_obj_set_style_border_width(btn, is_selected ? 3 : 1, 0);

        char op_title[24];
        std::snprintf(op_title, sizeof(op_title), "OPERATOR %u%s", op, has_fb ? " [FB]" : "");
        lv_obj_t* l1 = createLabel(btn, op_title, 4, 8, card_w - 8, &lv_font_montserrat_14, palette::kTextPrimary);
        lv_obj_set_style_text_align(l1, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t* l2 = createLabel(btn, is_carrier ? "CARRIER" : "MODULATOR", 4, 32, card_w - 8, &lv_font_montserrat_14, color);
        lv_obj_set_style_text_align(l2, LV_TEXT_ALIGN_CENTER, 0);

        const char* dest_desc = is_carrier ? "-> AUDIO OUT" : "-> MODULATION";
        lv_obj_t* l3 = createLabel(btn, dest_desc, 4, 58, card_w - 8, &lv_font_montserrat_14, palette::kTextSecondary);
        lv_obj_set_style_text_align(l3, LV_TEXT_ALIGN_CENTER, 0);

        attachLvContext<BigOpCtx>(btn, BigOpCtx{mctx, op}, [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
            auto* bctx = static_cast<BigOpCtx*>(lv_event_get_user_data(e));
            if (!bctx || !bctx->m || !bctx->m->backend) return;
            bctx->m->backend->requestParameterInt("fm.selected_op", bctx->op);
            for (unsigned j = 0; j < 6; ++j) {
                const unsigned curr_op = 6 - j;
                if (!bctx->m->op_btns[j]) continue;
                if (curr_op == bctx->op) {
                    lv_obj_set_style_border_color(bctx->m->op_btns[j], lv_color_hex(palette::kTextPrimary), 0);
                    lv_obj_set_style_border_width(bctx->m->op_btns[j], 3, 0);
                } else {
                    const auto t = smk::p4::fm::deriveTopology(bctx->m->backend->model().getInt("fm.algo", 1));
                    const uint32_t col = t.isCarrier(curr_op) ? palette::kActive : palette::kAccentPrimary;
                    lv_obj_set_style_border_color(bctx->m->op_btns[j], lv_color_hex(col), 0);
                    lv_obj_set_style_border_width(bctx->m->op_btns[j], 1, 0);
                }
            }
        }, LV_EVENT_CLICKED);
    }

    lv_obj_t* close_btn = createActionButton(root, "CLOSE", 556, 12, 140, 34, nullptr, nullptr);
    attachLvContext<BigModalCtx>(close_btn, BigModalCtx{&backend, root, {}}, [](lv_event_t* e) {
        if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
        auto* ctx = static_cast<BigModalCtx*>(lv_event_get_user_data(e));
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

    lv_obj_t* title_lbl = createLabel(panel, "TOPOLOGY: ALGO 01 / 32", 8, 2, w - 88, &lv_font_montserrat_14, palette::kTextPrimary);
    lv_obj_t* expand_btn = createActionButton(panel, "EXPAND", w - 74, 2, 66, 18, nullptr, nullptr);

    auto* ctx = attachLvContext<TopologyDiagramCtx>(
        panel,
        TopologyDiagramCtx{&backend, panel, title_lbl, expand_btn, nullptr, nullptr, {}, {}, {}, {}, {}},
        nullptr);

    // Preallocate 20 reusable LVGL line objects
    for (unsigned i = 0; i < 20; ++i) {
        lv_obj_t* line = lv_line_create(panel);
        ctx->lines[i] = line;
        lv_obj_add_flag(line, LV_OBJ_FLAG_HIDDEN);
    }

    // Create OUT node
    ctx->out_btn = createActionButton(panel, "", 428, 30, 44, 36, nullptr, nullptr);
    ctx->out_lbl = createLabel(ctx->out_btn, "OUT", 2, 8, 40, &lv_font_montserrat_14, palette::kActive);
    lv_obj_set_style_text_align(ctx->out_lbl, LV_TEXT_ALIGN_CENTER, 0);

    struct OpBtnCtx {
        TopologyDiagramCtx* dctx;
        unsigned op;
    };

    // Preallocate 6 operator nodes
    for (unsigned i = 0; i < 6; ++i) {
        const unsigned op = 6 - i;
        lv_obj_t* btn = createActionButton(panel, "", 10 + i * 72, 20, 54, 24, nullptr, nullptr);
        ctx->op_btns[i] = btn;

        char init_name[16];
        std::snprintf(init_name, sizeof(init_name), "OP %u", op);
        lv_obj_t* olbl = createLabel(btn, init_name, 2, 1, 50, &lv_font_montserrat_14, palette::kTextPrimary);
        lv_obj_set_style_text_align(olbl, LV_TEXT_ALIGN_CENTER, 0);
        ctx->op_lbls[i] = olbl;

        lv_obj_t* rlbl = createLabel(btn, "CARR", 2, 12, 50, &lv_font_montserrat_14, palette::kActive);
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