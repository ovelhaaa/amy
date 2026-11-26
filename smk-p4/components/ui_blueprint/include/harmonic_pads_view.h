#pragma once
#include "ui_widgets.h"
namespace smk::p4::ui {
class HarmonicPadsView {
public:
    ~HarmonicPadsView();
    bool build(lv_obj_t* parent,IUiBackend& backend);
    void refresh();
    void releaseAll();
private:
    struct Pad { HarmonicPadsView* owner=nullptr; unsigned id=0; lv_obj_t* button=nullptr; uint32_t token=0,requested_ms=0; bool down=false,rejected=false; char chord[24]{}; };
    static void event(lv_event_t* e);
    void release(Pad& pad);
    void labels();
    lv_obj_t* root_=nullptr,*controls_[4]{},*info_=nullptr;
    IUiBackend* backend_=nullptr;
    Pad pads_[8]{};
    static inline pads::Config config_{};
    bool hold_=false;
    uint32_t generation_=0;
    unsigned last_pad_=8;
};
}
