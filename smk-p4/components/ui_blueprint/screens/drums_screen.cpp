#include "ui_rhythm.h"
namespace smk::p4::ui {
namespace {
class DrumsScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::Drums; }
    bool build(lv_obj_t* parent,IUiBackend&,UiNavigationManager& nav) override {
        auto* panel=createRhythmPanel(parent,nav,id());
        createLabel(panel,"DRUMS",20,24,580,&lv_font_montserrat_28,palette::kTextPrimary);
        createLabel(panel,"PLANNED / NAO IMPLEMENTADO",20,88,580,&lv_font_montserrat_14,palette::kWarning);
        return true;
    }
    void refresh(IUiBackend&) override {}
};
}
ScreenView* createDrumsScreen() { return new DrumsScreen(); }
}
