#pragma once
#include "ui_screens.h"
#include "ui_widgets.h"
namespace smk::p4::ui {
inline lv_obj_t* createRhythmPanel(lv_obj_t* parent, UiNavigationManager& nav, ScreenId active) {
    struct Entry { const char* label; ScreenId screen; };
    constexpr Entry entries[]{{"SEQUENCER",ScreenId::Sequencer},{"ARP",ScreenId::Arpeggiator},
                              {"PATTERNS",ScreenId::Patterns},{"DRUMS",ScreenId::Drums}};
    struct Context { UiNavigationManager* nav; ScreenId screen; };
    createLabel(parent,"RITMO",12,12,140,&lv_font_montserrat_14,palette::kAccentPrimary);
    for(unsigned i=0;i<4;++i) {
        auto* button=createActionButton(parent,entries[i].label,8,42+int(i)*62,144,54,nullptr,nullptr,
                                        active==entries[i].screen,false);
        attachLvContext<Context>(button,{&nav,entries[i].screen},[](lv_event_t* event) {
            auto* ctx=static_cast<Context*>(lv_event_get_user_data(event));
            ctx->nav->requestNavigateTo(ctx->screen);
        },LV_EVENT_CLICKED);
    }
    auto* panel=lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_set_pos(panel,160,0);
    lv_obj_set_size(panel,640,388);
    lv_obj_remove_flag(panel,LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}
}
