#pragma once
#include "ui_widgets.h"
namespace smk::p4::ui {
class HarmonicPadsView {
public:
    ~HarmonicPadsView();
    bool build(lv_obj_t* parent,IUiBackend& backend);
    void refresh();
    void releaseAll();
#ifndef ESP_PLATFORM
    struct Updates { uint32_t labels=0,styles=0,heartbeats=0; };
    static inline Updates updates{0,0,0};
#endif
private:
    struct Pad { HarmonicPadsView* owner=nullptr; unsigned id=0; lv_obj_t* button=nullptr; uint32_t token=0,operation=0,requested_ms=0,color=UINT32_MAX; uint8_t input=0; bool down=false,minor=false; pads::Result result=pads::Result::None; char chord[24]{}; };
    static void event(lv_event_t* e);
    void release(Pad& pad);
    void labels();
    void selector(unsigned kind);
    bool octaveValid(unsigned octave) const;
    lv_obj_t* root_=nullptr,*controls_[4]{},*info_=nullptr,*context_=nullptr,*selector_=nullptr;
    IUiBackend* backend_=nullptr;
    Pad pads_[8]{};
    static inline pads::Config config_{};
    bool hold_=false;
    uint32_t generation_=0;
    unsigned last_pad_=8;
};
}
