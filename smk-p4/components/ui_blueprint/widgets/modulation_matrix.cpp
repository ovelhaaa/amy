#include "ui_widgets.h"
#include <cstdio>

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
    UiDemoBackend*       backend = nullptr;
    UiNavigationManager* nav     = nullptr;
    uint8_t              slot    = 0;
    uint8_t              field   = 0; // 0=Enable, 1=Src, 2=Dst, 3=Amt-, 4=Amt+, 5=Pol
};

} // namespace

lv_obj_t* createModulationMatrixWidget(lv_obj_t* parent, UiDemoBackend& backend, UiNavigationManager& nav,
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
        createLabel(box, abuf, 466, ry + 8, 68, &lv_font_montserrat_14, palette::kAccentPrimary);
        lv_obj_t* inc_btn = createActionButton(box, "+10", 536, ry, 44, row_h - 6, nullptr, nullptr);

        lv_obj_t* pol_btn = createActionButton(box, kPolNames[slot.polarity % 3], 586, ry, 106, row_h - 6, nullptr, nullptr);

        const FeatureStatus st = (slot_idx == 0 && slot.source == 1 && slot.dest == 1)
                                     ? FeatureStatus::Available
                                     : FeatureStatus::Planned;
        createStatusBadge(box, st, 700, ry + 8);

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
