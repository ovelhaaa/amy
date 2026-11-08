#include "ui_widgets.h"
#include <cstdio>
#include <initializer_list>

namespace smk::p4::ui {

namespace {

constexpr const char* kSrcNames[10] = {
    "OFF", "MOD WHL", "VELOCITY", "AFTOUCH", "LFO 1",
    "LFO 2", "ENV 2", "MACRO 1", "MACRO 2", "KEYTRK"
};

constexpr const char* kDstNames[10] = {
    "NONE", "OSC PIT", "OSC PWM", "FLT CUT", "FLT RES",
    "AMP LVL", "PAN", "LFO RAT", "FM INDX", "FX SEND"
};

constexpr const char* kPolNames[3] = {"UNI +", "BIP +/-", "INV -"};

struct ModRowCtx {
    IUiBackend*          backend = nullptr;
    UiNavigationManager* nav     = nullptr;
    uint8_t              slot    = 0;
    uint8_t              field   = 0; // 0=Enable, 1=Src, 2=Dst, 3=Amt-, 4=Amt+, 5=Pol
};

struct ModSlot1RefreshCtx {
    IUiBackend* backend    = nullptr;
    lv_obj_t*   en_btn     = nullptr;
    lv_obj_t*   src_btn    = nullptr;
    lv_obj_t*   dst_btn    = nullptr;
    lv_obj_t*   dec_btn    = nullptr;
    lv_obj_t*   inc_btn    = nullptr;
    lv_obj_t*   pol_btn    = nullptr;
    lv_obj_t*   amt_lbl    = nullptr;
    lv_obj_t*   badge_box  = nullptr;
};

} // namespace

lv_obj_t* createModulationMatrixWidget(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav,
                                       uint8_t page, int32_t x, int32_t y, int32_t w, int32_t h) {
    if (!parent) return nullptr;
    lv_obj_t* box = lv_obj_create(parent);
    if (!box) return nullptr;
    applyPanelStyle(box, false);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, w, h);

    // Table header
    createLabel(box, "SLOT", 8, 2, 54, &lv_font_montserrat_14, palette::kTextSecondary);
    createLabel(box, "SOURCE", 84, 2, 140, &lv_font_montserrat_14, palette::kTextSecondary);
    createLabel(box, "DESTINATION", 248, 2, 150, &lv_font_montserrat_14, palette::kTextSecondary);
    createLabel(box, "AMOUNT", 422, 2, 160, &lv_font_montserrat_14, palette::kTextSecondary);
    createLabel(box, "POLARITY", 602, 2, 100, &lv_font_montserrat_14, palette::kTextSecondary);
    createLabel(box, "STATUS", 704, 2, 68, &lv_font_montserrat_14, palette::kTextSecondary);

    const uint8_t base_slot = (page == 0) ? 0 : 4;
    const int32_t row_h = (h - 36) / 4;

    auto row_cb = [](lv_event_t* e) {
        if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
        auto* ctx = static_cast<ModRowCtx*>(lv_event_get_user_data(e));
        if (!ctx || !ctx->backend || !ctx->nav) return;
        if (ctx->backend->hasSoundEditor()) {
            if (ctx->slot != 0) return;
            const ModSlot& slot = ctx->backend->model().modSlot(0);
            switch (ctx->field) {
                case 0:
                    ctx->backend->requestParameterBool("mod.slot1.enable", !slot.enabled);
                    break;
                case 1: {
                    int32_t cur = slot.source;
                    for (int tries = 0; tries < 10; ++tries) {
                        cur = (cur + 1) % 10;
                        if (ctx->backend->isParameterValueSupported("mod.slot1.source", cur)) break;
                    }
                    ctx->backend->requestParameterInt("mod.slot1.source", cur);
                    break;
                }
                case 2: {
                    int32_t cur = slot.dest;
                    for (int tries = 0; tries < 10; ++tries) {
                        cur = (cur + 1) % 10;
                        if (ctx->backend->isParameterValueSupported("mod.slot1.dest", cur)) break;
                    }
                    ctx->backend->requestParameterInt("mod.slot1.dest", cur);
                    break;
                }
                case 3: {
                    const int32_t next_amt = (slot.amount <= -90) ? -100 : (int32_t(slot.amount) - 10);
                    ctx->backend->requestParameterFloat("mod.slot1.amount", static_cast<float>(next_amt));
                    break;
                }
                case 4: {
                    const int32_t next_amt = (slot.amount >= 90) ? 100 : (int32_t(slot.amount) + 10);
                    ctx->backend->requestParameterFloat("mod.slot1.amount", static_cast<float>(next_amt));
                    break;
                }
                case 5: {
                    const int32_t next_pol = (slot.polarity + 1) % 3;
                    ctx->backend->requestParameterInt("mod.slot1.polarity", next_pol);
                    break;
                }
            }
            return;
        }
        ModSlot slot = ctx->backend->model().modSlot(ctx->slot);
        switch (ctx->field) {
            case 0: slot.enabled = !slot.enabled; break;
            case 1: slot.source = static_cast<uint8_t>((slot.source + 1) % 10); break;
            case 2: slot.dest = static_cast<uint8_t>((slot.dest + 1) % 10); break;
            case 3: slot.amount = static_cast<int8_t>((slot.amount <= -90) ? -100 : (slot.amount - 10)); break;
            case 4: slot.amount = static_cast<int8_t>((slot.amount >= 90) ? 100 : (slot.amount + 10)); break;
            case 5: slot.polarity = static_cast<uint8_t>((slot.polarity + 1) % 3); break;
        }
        ctx->backend->model().setModSlot(ctx->slot, slot);
        ctx->nav->navigateTo(ctx->nav->currentScreen(), ctx->nav->subTab(ctx->nav->currentScreen()));
    };

    for (uint8_t r = 0; r < 4; ++r) {
        const uint8_t slot_idx = static_cast<uint8_t>(base_slot + r);
        const ModSlot& slot = backend.model().modSlot(slot_idx);
        const int32_t ry = 24 + r * row_h;

        char s_lbl[16];
        std::snprintf(s_lbl, sizeof(s_lbl), "M%u %s", unsigned(slot_idx + 1), slot.enabled ? "ON" : "OFF");
        lv_obj_t* en_btn = createActionButton(box, s_lbl, 6, ry, 72, row_h - 6, nullptr, nullptr, slot.enabled, false);
        lv_obj_t* src_btn = createActionButton(box, kSrcNames[slot.source % 10], 84, ry, 156, row_h - 6, nullptr, nullptr);
        lv_obj_t* dst_btn = createActionButton(box, kDstNames[slot.dest % 10], 246, ry, 166, row_h - 6, nullptr, nullptr);

        lv_obj_t* dec_btn = createActionButton(box, "-10", 418, ry, 44, row_h - 6, nullptr, nullptr);
        char abuf[16];
        std::snprintf(abuf, sizeof(abuf), "%+d%%", int(slot.amount));
        lv_obj_t* amt_lbl = createLabel(box, abuf, 466, ry + 8, 68, &lv_font_montserrat_14, palette::kAccentPrimary);
        lv_obj_t* inc_btn = createActionButton(box, "+10", 536, ry, 44, row_h - 6, nullptr, nullptr);

        lv_obj_t* pol_btn = createActionButton(box, kPolNames[slot.polarity % 3], 586, ry, 106, row_h - 6, nullptr, nullptr);

        const FeatureStatus st = backend.hasSoundEditor()
                                     ? ((slot_idx == 0) ? backend.parameterStatus("mod.slot1.dest") : FeatureStatus::Planned)
                                     : ((slot_idx == 0 && slot.source == 1 && slot.dest == 1)
                                            ? FeatureStatus::Available
                                            : FeatureStatus::Planned);
        lv_obj_t* badge_box = createStatusBadge(box, st, 700, ry + 8);

        if (backend.hasSoundEditor()) {
            const bool editable = (slot_idx == 0) && backend.isParameterEditable("mod.slot1.dest");
            if (!editable) {
                for (auto* button : {en_btn, src_btn, dst_btn, dec_btn, inc_btn, pol_btn})
                    if (button) lv_obj_add_state(button, LV_STATE_DISABLED);
            }
            if (slot_idx == 0) {
                attachLvContext<ModSlot1RefreshCtx>(
                    box,
                    ModSlot1RefreshCtx{&backend, en_btn, src_btn, dst_btn, dec_btn, inc_btn, pol_btn, amt_lbl, badge_box},
                    [](lv_event_t* e) {
                        auto* c = static_cast<ModSlot1RefreshCtx*>(lv_event_get_user_data(e));
                        if (!c || !c->backend) return;
                        const ModSlot& s = c->backend->model().modSlot(0);
                        if (c->en_btn) {
                            lv_obj_remove_style(c->en_btn, &theme().button_active, 0);
                            if (s.enabled) lv_obj_add_style(c->en_btn, &theme().button_active, 0);
                            if (lv_obj_t* l = lv_obj_get_child(c->en_btn, 0)) {
                                lv_label_set_text(l, s.enabled ? "M1 ON" : "M1 OFF");
                                lv_obj_set_style_text_color(l, lv_color_hex(s.enabled ? palette::kBackground : palette::kTextPrimary), 0);
                            }
                        }
                        if (c->src_btn) if (lv_obj_t* l = lv_obj_get_child(c->src_btn, 0)) lv_label_set_text(l, kSrcNames[s.source % 10]);
                        if (c->dst_btn) if (lv_obj_t* l = lv_obj_get_child(c->dst_btn, 0)) lv_label_set_text(l, kDstNames[s.dest % 10]);
                        if (c->pol_btn) if (lv_obj_t* l = lv_obj_get_child(c->pol_btn, 0)) lv_label_set_text(l, kPolNames[s.polarity % 3]);
                        if (c->amt_lbl) {
                            char ab[16];
                            std::snprintf(ab, sizeof(ab), "%+d%%", int(s.amount));
                            lv_label_set_text(c->amt_lbl, ab);
                        }
                        const FeatureStatus cur_st = c->backend->parameterStatus("mod.slot1.dest");
                        if (c->badge_box) {
                            lv_obj_set_style_bg_color(c->badge_box, lv_color_hex(featureStatusColor(cur_st)), 0);
                            if (lv_obj_t* bl = lv_obj_get_child(c->badge_box, 0)) {
                                lv_label_set_text_static(bl, featureStatusLabel(cur_st));
                                lv_obj_set_style_text_color(bl, lv_color_hex(cur_st == FeatureStatus::Unavailable ? palette::kTextPrimary : palette::kBackground), 0);
                            }
                        }
                        const bool ed = c->backend->isParameterEditable("mod.slot1.dest");
                        for (auto* btn : {c->en_btn, c->src_btn, c->dst_btn, c->dec_btn, c->inc_btn, c->pol_btn}) {
                            if (!btn) continue;
                            if (ed) lv_obj_remove_state(btn, LV_STATE_DISABLED);
                            else lv_obj_add_state(btn, LV_STATE_DISABLED);
                        }
                    },
                    LV_EVENT_REFRESH);
            }
        }

        if (en_btn)  attachLvContext<ModRowCtx>(en_btn,  ModRowCtx{&backend, &nav, slot_idx, 0}, row_cb, LV_EVENT_CLICKED);
        if (src_btn) attachLvContext<ModRowCtx>(src_btn, ModRowCtx{&backend, &nav, slot_idx, 1}, row_cb, LV_EVENT_CLICKED);
        if (dst_btn) attachLvContext<ModRowCtx>(dst_btn, ModRowCtx{&backend, &nav, slot_idx, 2}, row_cb, LV_EVENT_CLICKED);
        if (dec_btn) attachLvContext<ModRowCtx>(dec_btn, ModRowCtx{&backend, &nav, slot_idx, 3}, row_cb, LV_EVENT_CLICKED);
        if (inc_btn) attachLvContext<ModRowCtx>(inc_btn, ModRowCtx{&backend, &nav, slot_idx, 4}, row_cb, LV_EVENT_CLICKED);
        if (pol_btn) attachLvContext<ModRowCtx>(pol_btn, ModRowCtx{&backend, &nav, slot_idx, 5}, row_cb, LV_EVENT_CLICKED);
    }

    return box;
}

} // namespace smk::p4::ui
