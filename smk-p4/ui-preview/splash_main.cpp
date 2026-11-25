#include "preview_harness.h"
#include "p4my_splash.h"
#include <cassert>
#include <chrono>
#include <cstring>
using namespace smk::p4::ui;
using namespace smk::p4::ui::preview;
static void capture(const char* directory,const char* name) {
    lv_refr_now(nullptr);
    char path[512]; std::snprintf(path,sizeof(path),"%s/%s.ppm",directory,name);
    assert(saveFramebufferPpm(path));
}
int main(int argc,char** argv) {
    const char* dir=argc>1?argv[1]:"build/p4my_splash";
    initHostLvgl();
    auto* performance=lv_label_create(lv_screen_active());
    lv_label_set_text(performance,"PERFORMANCE - existing screen preserved");
    lv_refr_now(nullptr);
    lv_mem_monitor_t before{},during{},after{};
    lv_mem_monitor(&before);
    const unsigned initial_used=before.total_size-before.free_size;
    P4mySplash splash;
    assert(splash.begin(lv_screen_active(),0));
    lv_mem_monitor(&during);
    const unsigned times[]={0,700,1400,2700};
    const char* names[]={"01_initial","02_waves","03_logo","04_final"};
    for(unsigned i=0;i<4;++i) { splash.seek(times[i]); capture(dir,names[i]); }
    double total_us=0,max_us=0;
    for(unsigned frame=0;frame<=90;++frame) {
        const auto start=std::chrono::steady_clock::now();
        splash.seek(frame*33); lv_refr_now(nullptr);
        const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
        total_us+=us; if(us>max_us) max_us=us;
        char name[32]; std::snprintf(name,sizeof(name),"frame_%03u",frame); capture(dir,name);
    }
    splash.finish(100); splash.finish(200); splash.service(279); assert(splash.active());
    splash.service(280); assert(!splash.active());
    assert(lv_obj_is_valid(performance));
    capture(dir,"05_performance_return");
    // Warm up style/render caches, then measure repeated complete lifecycle.
    lv_refr_now(nullptr); lv_mem_monitor(&before);
    auto* timer_before=lv_timer_get_next(nullptr);
    for(unsigned i=0;i<50;++i) {
        P4mySplash::Config c; c.fps=i%2?20:30;
        assert(splash.begin(lv_screen_active(),0xFFFFFFF0u,c));
        splash.service(30); splash.finish(30); splash.service(210);
        assert(!splash.active());
    }
    lv_refr_now(nullptr); lv_mem_monitor(&after);
    assert(before.total_size-before.free_size==after.total_size-after.free_size);
    assert(lv_timer_get_next(nullptr)==timer_before);
    auto* parent=lv_obj_create(lv_screen_active());
    assert(splash.begin(parent,0)); lv_obj_delete(parent);
    assert(!splash.active()); splash.service(3000); splash.close();
    P4mySplash::Config invalid; invalid.fps=25;
    assert(!splash.begin(lv_screen_active(),0,invalid));
    lv_mem_monitor_t available{}; lv_mem_monitor(&available);
    void* pressure=lv_malloc(available.free_biggest_size-4096);
    assert(pressure);
    assert(!splash.begin(lv_screen_active(),0));
    lv_free(pressure);
    assert(lv_obj_is_valid(performance));
    std::printf("PASS: low-memory refusal; manual timeline, 20/30 FPS, early exit, wrap, parent delete, 50 lifecycles, no timers, Performance retained\n");
    std::printf("MEMORY object=%zu bytes; initial LVGL delta=%u bytes; repeated lifecycle leak=0 bytes\n",sizeof(splash),unsigned((during.total_size-during.free_size)-initial_used));
    std::printf("HOST seek+software-render avg=%.1f us max=%.1f us (91 frames; no physical performance claim)\n",total_us/91,max_us);
    lv_obj_delete(performance);
    lv_deinit();
}
