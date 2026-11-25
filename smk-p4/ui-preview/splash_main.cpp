#include "preview_harness.h"
#include "p4my_splash.h"
#include "usb_input.h"
#include <cassert>
#include <chrono>
#include <cstring>

using namespace smk::p4::ui;
using namespace smk::p4::ui::preview;

namespace {
void capture(const char* directory, const char* name) {
    lv_refr_now(nullptr);
    char path[512];
    std::snprintf(path, sizeof(path), "%s/%s.ppm", directory, name);
    assert(saveFramebufferPpm(path));
}
} // namespace

int main(int argc, char** argv) {
    const char* dir = argc > 1 ? argv[1] : "build/p4my_splash";
    initHostLvgl();

    // Underlying Performance button to verify touch on splash never propagates underneath.
    auto* underlying_btn = lv_button_create(lv_screen_active());
    lv_obj_set_pos(underlying_btn, 0, 0);
    lv_obj_set_size(underlying_btn, 800, 480);
    unsigned underlying_clicks = 0;
    lv_obj_add_event_cb(underlying_btn, [](lv_event_t* e) {
        auto* count = static_cast<unsigned*>(lv_event_get_user_data(e));
        ++(*count);
    }, LV_EVENT_ALL, &underlying_clicks);
    auto* performance = lv_label_create(underlying_btn);
    lv_label_set_text(performance, "P4my PERFORMANCE - existing screen preserved");
    lv_refr_now(nullptr);

    lv_mem_monitor_t before{}, during{}, after{};
    lv_mem_monitor(&before);
    const unsigned initial_used = before.total_size - before.free_size;

    P4mySplash splash;
    assert(splash.begin(lv_screen_active(), 0));
    lv_mem_monitor(&during);

    // Capture milestones + 91 animation frames for GIF & replay.html
    const unsigned times[] = {0, 700, 1400, 2700};
    const char* names[] = {"01_initial", "02_waves", "03_logo", "04_final"};
    for (unsigned i = 0; i < 4; ++i) {
        splash.seek(times[i]);
        capture(dir, names[i]);
    }
    double total_us = 0, max_us = 0;
    for (unsigned frame = 0; frame <= 90; ++frame) {
        const auto start = std::chrono::steady_clock::now();
        splash.seek(frame * 33);
        lv_refr_now(nullptr);
        const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
        total_us += us;
        if (us > max_us) max_us = us;
        char name[32];
        std::snprintf(name, sizeof(name), "frame_%03u", frame);
        capture(dir, name);
    }

    // 1. Engine ready BEFORE splash creation -> still plays full 2700ms + 250ms hold
    assert(splash.begin(lv_screen_active(), 0));
    splash.setEngineReady(true, 0);
    splash.service(1000);
    assert(splash.active() && splash.state() == P4mySplash::State::Animating);
    splash.service(2700);
    assert(splash.active() && splash.state() == P4mySplash::State::FinalHold);
    splash.service(2949);
    assert(splash.active() && splash.state() == P4mySplash::State::FinalHold);
    splash.service(2950);
    assert(splash.active() && splash.state() == P4mySplash::State::Exiting);
    splash.service(2950 + 180);
    assert(!splash.active() && splash.state() == P4mySplash::State::Closed);
    assert(splash.dismissReason() == P4mySplash::DismissReason::AnimationComplete);

    // 2. Engine ready DURING animation
    assert(splash.begin(lv_screen_active(), 100));
    splash.service(1500);
    splash.setEngineReady(true, 1500);
    assert(splash.active() && splash.state() == P4mySplash::State::Animating);
    splash.service(2800);
    assert(splash.state() == P4mySplash::State::FinalHold);
    splash.service(3050);
    assert(splash.state() == P4mySplash::State::Exiting);
    splash.service(3230);
    assert(!splash.active());

    // 3. Engine ready AFTER animation (WaitingForEngine) + 11/12: zero wave recalc during static wait
    assert(splash.begin(lv_screen_active(), 0));
    splash.service(2700);
    assert(splash.state() == P4mySplash::State::FinalHold);
    splash.service(2950);
    assert(splash.state() == P4mySplash::State::WaitingForEngine);
    const uint32_t waves_at_wait = splash.metrics().wave_updates;
    for (uint32_t t = 3000; t <= 4500; t += 33) {
        splash.service(t);
    }
    assert(splash.metrics().wave_updates == waves_at_wait); // Frozen! Zero trig recalculation!
    capture(dir, "07_static_hold");
    splash.setEngineReady(true, 4550);
    splash.service(4550);
    assert(splash.state() == P4mySplash::State::Exiting);
    splash.service(4730);
    assert(!splash.active());

    // 4. Engine remaining unavailable -> bounded wait_timeout_ms exits cleanly
    P4mySplash::Config wait_cfg;
    wait_cfg.wait_timeout_ms = 500;
    assert(splash.begin(lv_screen_active(), 0, wait_cfg));
    splash.service(2700);
    splash.service(2950);
    assert(splash.state() == P4mySplash::State::WaitingForEngine);
    splash.service(3450);
    assert(splash.state() == P4mySplash::State::Exiting);
    splash.service(3630);
    assert(!splash.active());

    // 5. Initialization error -> immediate exit
    assert(splash.begin(lv_screen_active(), 0));
    splash.service(300);
    splash.setEngineError(true, 300);
    assert(splash.state() == P4mySplash::State::Exiting);
    assert(splash.dismissReason() == P4mySplash::DismissReason::EngineError);
    splash.service(480);
    assert(!splash.active());

    // 6. Early dismiss by touch + non-propagation to underlying Performance controls
    assert(splash.begin(lv_screen_active(), 0));
    splash.service(600);
    const unsigned clicks_before = underlying_clicks;
    lv_obj_send_event(splash.object(), LV_EVENT_PRESSED, nullptr);
    lv_obj_send_event(splash.object(), LV_EVENT_RELEASED, nullptr);
    lv_obj_send_event(splash.object(), LV_EVENT_CLICKED, nullptr);
    assert(underlying_clicks == clicks_before); // Touch did NOT propagate!
    assert(splash.state() == P4mySplash::State::Exiting);
    assert(splash.dismissReason() == P4mySplash::DismissReason::Touch);
    splash.service(690);
    capture(dir, "06_early_dismiss");
    splash.service(780);
    assert(!splash.active());
    assert(lv_obj_is_valid(performance));
    capture(dir, "05_performance_return");

    // 7/8/9. MIDI Note On (vel>0) dismisses; vel=0, Clock (0xF8), Active Sensing (0xFE) do NOT
    smk::p4::amy::UsbInput usb;
    const uint32_t base_note_ons = usb.note_on_events.load();
    const uint8_t clock_pkt[]{0x0F, 0xF8, 0x00, 0x00};
    const uint8_t sense_pkt[]{0x0F, 0xFE, 0x00, 0x00};
    const uint8_t vel0_pkt[]{0x09, 0x90, 60, 0};
    const uint8_t noteon_pkt[]{0x09, 0x90, 60, 100};
    assert(usb.transfer(clock_pkt, sizeof(clock_pkt), 10));
    assert(usb.transfer(sense_pkt, sizeof(sense_pkt), 20));
    assert(usb.transfer(vel0_pkt, sizeof(vel0_pkt), 30));
    assert(usb.note_on_events.load() == base_note_ons);
    assert(usb.transfer(noteon_pkt, sizeof(noteon_pkt), 40));
    assert(usb.note_on_events.load() == base_note_ons + 1);
    smk::p4::amy::Command cmd{};
    assert(usb.commands.pop(cmd) && cmd.type == smk::p4::amy::CommandType::NoteOff && cmd.id == 60);
    assert(usb.commands.pop(cmd) && cmd.type == smk::p4::amy::CommandType::NoteOn && cmd.id == 60 && cmd.value == 100);

    assert(splash.begin(lv_screen_active(), 0));
    splash.service(500);
    splash.requestDismiss(P4mySplash::DismissReason::MidiNoteOn, 500);
    assert(splash.state() == P4mySplash::State::Exiting);
    assert(splash.dismissReason() == P4mySplash::DismissReason::MidiNoteOn);

    // 10. Repeated dismiss requests (idempotent)
    splash.finish(550);
    splash.requestDismiss(P4mySplash::DismissReason::Touch, 560);
    assert(splash.dismissReason() == P4mySplash::DismissReason::MidiNoteOn);
    splash.service(680);
    assert(!splash.active());

    // 14/15/16/17. Repeated 50 lifecycles, 20/30 FPS, tick wrap, parent delete, invalid config, low-memory refusal
    lv_refr_now(nullptr);
    lv_mem_monitor(&before);
    auto* timer_before = lv_timer_get_next(nullptr);
    for (unsigned i = 0; i < 50; ++i) {
        P4mySplash::Config c;
        c.fps = (i % 2) ? 20 : 30;
        assert(splash.begin(lv_screen_active(), 0xFFFFFFF0u, c));
        splash.service(30);
        splash.finish(30);
        splash.service(210);
        assert(!splash.active());
    }
    lv_refr_now(nullptr);
    lv_mem_monitor(&after);
    assert(before.total_size - before.free_size == after.total_size - after.free_size);
    assert(lv_timer_get_next(nullptr) == timer_before);

    auto* parent = lv_obj_create(lv_screen_active());
    assert(splash.begin(parent, 0));
    lv_obj_delete(parent);
    assert(!splash.active());
    splash.service(3000);
    splash.close();

    P4mySplash::Config invalid;
    invalid.fps = 25;
    assert(!splash.begin(lv_screen_active(), 0, invalid));

    lv_mem_monitor_t available{};
    lv_mem_monitor(&available);
    void* pressure = lv_malloc(available.free_biggest_size - 4096);
    assert(pressure);
    assert(!splash.begin(lv_screen_active(), 0));
    lv_free(pressure);
    assert(lv_obj_is_valid(performance));

    std::printf("PASS: 18/18 splash lifecycle, touch non-propagation, MIDI NoteOn filter, freeze, 20/30 FPS, wrap, 50 lifecycles\n");
    std::printf("MEMORY object=%zu bytes; initial LVGL delta=%u bytes; repeated lifecycle leak=0 bytes\n",
                sizeof(splash), unsigned((during.total_size - during.free_size) - initial_used));
    std::printf("HOST seek+software-render avg=%.1f us max=%.1f us (91 frames; no physical performance claim)\n",
                total_us / 91.0, max_us);
    lv_obj_delete(underlying_btn);
    lv_deinit();
    return 0;
}
