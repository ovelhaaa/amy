#include "ui_screens.h"
#include "ui_widgets.h"
#include <cstdio>
#include <cstring>

namespace smk::p4::ui {
namespace {
class EffectsScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::Effects; }

    bool build(lv_obj_t* parent,IUiBackend& backend,UiNavigationManager&) override {
        createLabel(parent,"DELAY",8,5,130,&lv_font_montserrat_14,palette::kAccentPrimary);
        timing_=createLabel(parent,"",112,5,678,&lv_font_montserrat_14,palette::kTextPrimary);
        constexpr const char* delay[]{
            "fx.delay.mode","fx.delay.division","fx.delay.time_ms","fx.delay.feedback","fx.delay.mix"
        };
        for(unsigned i=0;i<5;++i) createParameterCard(parent,backend,delay[i],8+158*i,28,152,108);
        notice_=createLabel(parent,"",8,144,784,&lv_font_montserrat_14,palette::kAccentPrimary);
        createLabel(parent,"CHORUS                  REVERB                  EQ",8,175,784,
                    &lv_font_montserrat_14,palette::kTextSecondary);
        constexpr const char* other[]{
            "fx.chorus.mode","fx.chorus.depth","fx.reverb.size","fx.reverb.mix","fx.master_tone","fx.eq.low_db"
        };
        for(unsigned i=0;i<6;++i) createParameterCard(parent,backend,other[i],8+198*(i%4),200+94*(i/4),190,90);
        refresh(backend);
        return true;
    }

    void refresh(IUiBackend& backend) override {
        IUiBackend::DelayStatus status{};
        if(!backend.delayStatus(status)) return;
        char timing[100];
        const int bpm=backend.model().getInt("perf.bpm",120);
        if(status.sync)
            std::snprintf(timing,sizeof(timing),"%d BPM  |  TARGET %.2f ms  |  DSP %.2f ms",
                          bpm,double(status.target_ms),double(status.applied_ms));
        else std::snprintf(timing,sizeof(timing),"%d BPM  |  DSP %.2f ms",bpm,double(status.applied_ms));
        setLabel(timing_,timing);
        setLabel(notice_,status.bypassed?"FX BYPASSED":status.out_of_range?
                 "SYNC OUT OF RANGE  |  HOLDING SAFE TIME  |  TIME MACROS PAUSED":
                 status.sync?"DELAY TIME MACROS PAUSED":"");
    }
private:
    static void setLabel(lv_obj_t* label,const char* text) {
        if(label && std::strcmp(lv_label_get_text(label),text)) lv_label_set_text(label,text);
    }
    lv_obj_t* timing_=nullptr;
    lv_obj_t* notice_=nullptr;
};
}
ScreenView* createEffectsScreen() { return new EffectsScreen(); }
}
