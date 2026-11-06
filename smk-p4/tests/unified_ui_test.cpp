#include "ui_blueprint_performance_adapter.h"
#include "local_performance_backend.h"
#include "../ui-preview/preview_harness.h"
#include <cassert>
#include <cstdio>
extern "C" void probe_fail_next(unsigned);
using namespace smk::p4;
using namespace smk::p4::ui;
int main() {
    preview::initHostLvgl();amy::Runtime r;assert(r.begin());
    LocalAmyPerformanceBackend b(r.commands());UiBlueprintPerformanceAdapter a(b);
    auto publish=[&] { b.publish(r.snapshot(),false,0,0);a.service(++preview::g_sim_tick_ms); };
    publish();UiBlueprint app(a);assert(app.begin());
    for(unsigned i=0;i<unsigned(ScreenId::Count);++i) {
        assert(app.navigation().navigateTo(static_cast<ScreenId>(i)));
        preview::pumpFrame(app);
    }
    assert(b.patchCount()==15 && a.model().diagnostics().max_voices==8);
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
    assert(!a.isPatchSelectable(32) && !a.isPatchSelectable(255));
    assert(a.requestSelectPatch(135).accepted());r.apply(1);
    probe_fail_next(1);r.loadPendingPatch();publish();
    assert(a.lastCommandState()==UiCommandState::Rejected && b.patchError());
    assert(!a.requestNoteOn(60,100).accepted());
    assert(a.requestPanic().accepted());r.apply(1);
    assert(a.requestSelectPatch(128).accepted());r.apply(1);r.loadPendingPatch();publish();
    assert(!b.patchError() && a.model().activePatch().id==128);
    assert(app.navigation().navigateTo(ScreenId::PatchBrowser));preview::pumpFrame(app);
    assert(preview::saveFramebufferPpm("build/unified08/browser.ppm"));
    app.shutdown();r.end();
    std::puts("PASS unified live adapter + LVGL9.3:14 screens,15 patches,requested/loading/confirmed,failure/retry,Panic,favorites,unsupported rejection");
}
