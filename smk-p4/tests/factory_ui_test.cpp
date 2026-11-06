#include "ui_blueprint_performance_adapter.h"
#include "local_performance_backend.h"
#include "../ui-preview/preview_harness.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
#include <cstring>
extern "C" void probe_fail_next(unsigned);
using namespace smk::p4;
using namespace smk::p4::ui;
static lv_obj_t* findButtonWithLabel(lv_obj_t* root,const char* text){
    for(unsigned i=0;i<lv_obj_get_child_count(root);++i){auto* child=lv_obj_get_child(root,i);
        if(lv_obj_check_type(child,&lv_label_class) && std::strcmp(lv_label_get_text(child),text)==0)return lv_obj_get_parent(child);
        if(auto* found=findButtonWithLabel(child,text))return found;
    }return nullptr;
}
int main() {
    preview::initHostLvgl();amy::Runtime r;assert(r.begin());
    LocalAmyPerformanceBackend b(r.commands());UiBlueprintPerformanceAdapter a(b);
    auto publish=[&] { b.publish(r.snapshot(),false,0,0);a.service(++preview::g_sim_tick_ms); };
    publish();UiBlueprint app(a);assert(app.begin());
    for(unsigned i=0;i<unsigned(ScreenId::Count);++i) {
        assert(app.navigation().navigateTo(static_cast<ScreenId>(i)));
        preview::pumpFrame(app);
    }
    assert(b.patchCount()==amy::kPatchCount && a.model().diagnostics().max_voices==8);
    for(unsigned i=0;i<b.patchCount();++i) {
        const auto id=b.patchAt(i);assert(a.isPatchSelectable(id));
        const auto old=a.model().activePatch().id;
        assert(a.requestSelectPatch(id).state==UiCommandState::Requested);
        publish();assert(a.lastCommandState()==UiCommandState::Requested);
        assert(a.model().activePatch().id==old);
        r.apply(1);publish();assert(a.isBusy());
        assert(!a.requestNoteOn(60,100).accepted());
        r.loadPendingPatch();publish();
        assert(!a.isBusy() && a.lastCommandState()==UiCommandState::Confirmed);
        assert(a.model().activePatch().id==id);
        const bool favorite=a.model().isFavorite(id);a.model().toggleFavorite(id);
        assert(a.model().isFavorite(id)!=favorite);
    }
    for(unsigned id=0;id<256;++id) assert(a.isPatchSelectable(id)==(amy::patchIndex(id)>=0));
    assert(!a.isPatchSelectable(256));
    assert(a.requestSelectPatch(135).accepted());r.apply(1);
    probe_fail_next(1);r.loadPendingPatch();publish();
    assert(a.lastCommandState()==UiCommandState::Rejected && b.patchError());
    assert(!a.requestNoteOn(60,100).accepted());
    assert(a.requestPanic().accepted());r.apply(1);
    assert(a.requestSelectPatch(128).accepted());r.apply(1);r.loadPendingPatch();publish();
    assert(!b.patchError() && a.model().activePatch().id==128);
    assert(app.navigation().navigateTo(ScreenId::PatchBrowser));preview::pumpFrame(app);
    assert(preview::saveFramebufferPpm("build/factory010/browser.ppm"));
    auto click=[&](const char* text){auto* button=findButtonWithLabel(app.contentObject(),text);assert(button);lv_obj_send_event(button,LV_EVENT_CLICKED,nullptr);preview::pumpFrame(app);};
    for(unsigned page=0;page<44;++page)click("PAGE >");
    for(unsigned page=0;page<44;++page)click("< PAGE");
    for(const char* bank:{"JUNO A","JUNO B","AMY DX7","FAVORITES *","ALL BANKS"}){click(bank);for(unsigned page=0;page<5;++page)click("PAGE >");}
    click("BASS");assert(a.model().browser_category==PatchCategory::Bass);
    click("STRINGS");assert(a.model().browser_category==PatchCategory::Strings);
    click("ALL");assert(a.model().browser_category==PatchCategory::All);
    for(unsigned i=0;i<256;++i){a.model().toggleFavorite(i);a.model().toggleFavorite(i);}
    assert(a.model().activePatch().id==128);
    a.model().browser_bank=1;
    uint16_t expected=UINT16_MAX;
    size_t count=0;const auto* catalog=UiModel::catalog(count);assert(count==256);
    for(size_t i=0;i<count;++i)if(a.isPatchSelectable(catalog[i].id) && a.model().matchesBrowser(catalog[i])){expected=catalog[i].id;break;}
    assert(app.navigation().navigateTo(ScreenId::Performance));preview::pumpFrame(app);
    if(expected!=UINT16_MAX){click("NEXT >");assert(a.model().activePatch().id==128);r.apply(1);r.loadPendingPatch();publish();assert(a.model().activePatch().id==expected);}
    a.model().browser_bank=0;
    assert(app.navigation().navigateTo(ScreenId::PatchBrowser));preview::pumpFrame(app);
    assert(lv_mem_test()==LV_RESULT_OK);
    assert(preview::saveFramebufferPpm("build/factory010/browser_last.ppm"));
    app.shutdown();r.end();
    std::printf("PASS factory live adapter + LVGL9.3:14 screens,256 catalog entries,%zu admitted patches,44 pages/banks/favorites,filtered PrevNext,requested/loading/confirmed,failure/retry,Panic,unsupported rejection\n",amy::kPatchCount);
}
