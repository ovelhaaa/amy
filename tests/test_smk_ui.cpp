#include <cstdio>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>

// Include UI headers
#include "display_driver.h"
#include "display_layout.h"
#include "navigation_model.h"
#include "dummy_display_driver.h"
#include "font_renderer.h"
#include "widgets.h"
#include "ui_theme.h"

// Screen headers
#include "screens/home_screen.h"
#include "screens/sequencer_screen.h"
#include "screens/parameter_screen.h"
#include "screens/pad_screen.h"
#include "screens/midi_monitor_screen.h"
#include "screens/scene_screen.h"
#include "screens/splash_screen.h"
#include "screens/system_screen.h"
#include "screens/midi_learn_screen.h"
#include "diagnostics.h"
#include "step_sequencer.h"
#include "scene_manager.h"

namespace smk {
uint16_t StepSequencer::getTrackStepMask(uint8_t t) const { return 0x1511; }
uint16_t StepSequencer::getTrackPlockMask(uint8_t t) const { return 0x0010; }
bool StepSequencer::isTrackMuted(uint8_t t) const { return false; }
const char* StepSequencer::trackName(uint8_t t) const {
    static const char* names[] = {"BD", "SD", "CH", "OH"};
    return names[t % 4];
}
const Scene& SceneManager::scene(uint8_t idx) const {
    static Scene s{};
    return s;
}
const char* MidiLearn::currentStepName() const { return "MOVE CONTROL"; }
const char* MidiLearn::currentStepHint() const { return "SEND THE EXPECTED MIDI EVENT"; }
Diagnostics& Diagnostics::instance() {
    static Diagnostics diagnostics;
    return diagnostics;
}
Diagnostics::Snapshot Diagnostics::takeSnapshot() const {
    return Snapshot{0, 1200, 800, 48000, 128 * 1024, 4 * 1024 * 1024,
                    96 * 1024, 3 * 1024 * 1024, 240, 16 * 1024 * 1024,
                    8 * 1024 * 1024, 12.5f, 4, 0, 128, 0, 1, 2, 1, true, "test"};
}
}

using namespace smk;

class BoundsCheckingDisplayDriver : public DummyDisplayDriver {
public:
    BoundsCheckingDisplayDriver(int16_t width, int16_t height)
        : DummyDisplayDriver(width, height) {}

    void drawPixel(int16_t x, int16_t y, uint16_t color) override {
        if (x < 0 || y < 0 || x >= width() || y >= height()) {
            ++out_of_bounds_count_;
            return;
        }
        DummyDisplayDriver::drawPixel(x, y, color);
    }

    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
        if (w < 0 || h < 0 || x < 0 || y < 0 || x + w > width() || y + h > height()) {
            ++out_of_bounds_count_;
        }
        DummyDisplayDriver::fillRect(x, y, w, h, color);
    }

    uint32_t outOfBoundsCount() const { return out_of_bounds_count_; }

private:
    uint32_t out_of_bounds_count_{0};
};

void testGeometryAndContract() {
    printf("[TEST] DisplayDriver Geometry and Pure-Virtual Contract...\n");

    DummyDisplayDriver wide_display(284, 76);
    assert(wide_display.width() == 284);
    assert(wide_display.height() == 76);
    assert(wide_display.begin());

    DummyDisplayDriver compact_display(160, 128);
    assert(compact_display.width() == 160);
    assert(compact_display.height() == 128);
    assert(compact_display.begin());

    DummyDisplayDriver custom_display(240, 240);
    assert(custom_display.width() == 240);
    assert(custom_display.height() == 240);
    assert(custom_display.begin());

    static_assert(classifyDisplayLayout(160, 128) == DisplayLayoutClass::Compact);
    static_assert(classifyDisplayLayout(240, 240) == DisplayLayoutClass::Square);
    static_assert(classifyDisplayLayout(284, 76) == DisplayLayoutClass::Widescreen);
    static_assert(displayFrameIntervalMs(160, 128) == 33);
    static_assert(displayFrameIntervalMs(284, 76) == 33);
    static_assert(displayFrameIntervalMs(240, 240) == 66);

    static_assert(nextPerformancePage(ScreenId::Home) == ScreenId::Sequencer);
    static_assert(nextPerformancePage(ScreenId::Sequencer) == ScreenId::Pads);
    static_assert(nextPerformancePage(ScreenId::Pads) == ScreenId::Scenes);
    static_assert(nextPerformancePage(ScreenId::Scenes) == ScreenId::Home);
    static_assert(previousPerformancePage(ScreenId::Home) == ScreenId::Scenes);
    static_assert(previousPerformancePage(ScreenId::Sequencer) == ScreenId::Home);
    static_assert(previousPerformancePage(ScreenId::Pads) == ScreenId::Sequencer);
    static_assert(previousPerformancePage(ScreenId::Scenes) == ScreenId::Pads);
    static_assert(nextPerformancePage(ScreenId::MidiLearn) == ScreenId::Home);

    printf("  -> Geometry contract verified for multiple display resolutions.\n");
}

void testDirtyRectTracking() {
    printf("[TEST] Dirty Rectangle Tracking & Conditional Flush...\n");

    DummyDisplayDriver display(284, 76);
    assert(display.begin());
    display.clearDirty();
    assert(!display.isDirty());
    assert(display.dirtyRect().isEmpty());

    // Flush when clean does not transmit
    uint32_t f0 = display.flushCount();
    display.flush();
    assert(display.flushCount() == f0);

    // Draw single pixel
    display.drawPixel(50, 20, DisplayDriver::kColorWhite);
    assert(display.isDirty());
    Rect dr = display.dirtyRect();
    assert(dr.x == 50 && dr.y == 20 && dr.w == 1 && dr.h == 1);

    // Draw another pixel and check merged bounding box
    display.drawPixel(60, 30, DisplayDriver::kColorCyan);
    dr = display.dirtyRect();
    assert(dr.x == 50 && dr.y == 20 && dr.w == 11 && dr.h == 11);

    // Flush clears dirty state and increments flush count
    display.flush();
    assert(!display.isDirty());
    assert(display.flushCount() == f0 + 1);

    // FillRect marks exact subregion dirty
    display.fillRect(10, 15, 40, 25, DisplayDriver::kColorYellow);
    assert(display.isDirty());
    dr = display.dirtyRect();
    assert(dr.x == 10 && dr.y == 15 && dr.w == 40 && dr.h == 25);
    display.flush();

    // FillScreen invalidates full frame
    display.fillScreen(DisplayDriver::kColorBlack);
    assert(display.isDirty());
    dr = display.dirtyRect();
    assert(dr.x == 0 && dr.y == 0 && dr.w == 284 && dr.h == 76);
    display.flush();
    assert(!display.isDirty());

    printf("  -> Dirty rect tracking and conditional flush verified successfully.\n");
}

void testFontRenderer() {
    printf("[TEST] FontRenderer string metrics...\n");

    int16_t w_3x5 = FontRenderer::stringWidth("ABC", FontType::Font3x5);
    assert(w_3x5 == 3 * 4); // 12px

    int16_t w_5x7 = FontRenderer::stringWidth("ABC", FontType::Font5x7);
    assert(w_5x7 == 3 * 6); // 18px

    int16_t h_5x7 = FontRenderer::fontHeight(FontType::Font5x7);
    assert(h_5x7 == 7);

    DummyDisplayDriver display(284, 76);
    display.begin();
    FontRenderer::drawString(display, 0, 0, "TEST STRING", DisplayDriver::kColorWhite, DisplayDriver::kColorBlack, FontType::Font5x7);
    assert(display.isDirty());

    printf("  -> FontRenderer verified.\n");
}

void testOscilloscopeWidget() {
    printf("[TEST] OscilloscopeWidget Auto-Gain & Bounds...\n");

    DummyDisplayDriver display(284, 76);
    display.begin();

    OscilloscopeWidget scope(10, 10, 50, 20);
    
    // Silence
    scope.draw(display);

    // Audio test waveform (sine burst)
    int16_t test_wave[64];
    for (int i = 0; i < 64; ++i) {
        test_wave[i] = static_cast<int16_t>(sinf(i * 0.2f) * 16000.0f);
    }
    scope.setSamples(test_wave, 64);
    scope.setActive(true);
    scope.draw(display);

    assert(display.isDirty());
    printf("  -> OscilloscopeWidget rendering verified.\n");
}

void testWidgets() {
    printf("[TEST] Standard UI Widgets (Label, ProgressBar, BarGauge, BorderBox, StatusIndicator)...\n");

    DummyDisplayDriver display(284, 76);
    display.begin();

    Label lbl(0, 0, "TEST LABEL");
    lbl.draw(display);

    ProgressBar pb(0, 10, 100, 10);
    pb.setValue(0.75f);
    pb.draw(display);

    BarGauge bg(0, 25, 32, 45, "CUTOFF");
    bg.setValue(90);
    bg.draw(display);

    BorderBox bb(50, 0, 100, 50, "TITLE");
    bb.draw(display);

    StatusIndicator si(0, 0, "USB");
    si.setActive(true);
    si.draw(display);

    assert(display.isDirty());
    printf("  -> Widgets verified.\n");
}

void testMultiScreenRendering() {
    printf("[TEST] Multi-Screen Rendering (160x128 Compact, 240x240 Square & 284x76 Wide)...\n");

    DummyDisplayDriver wide_display(284, 76);
    wide_display.begin();

    DummyDisplayDriver compact_display(160, 128);
    compact_display.begin();

    BoundsCheckingDisplayDriver square_display(240, 240);
    square_display.begin();

    // 1. HomeScreen
    {
        HomeScreen home;
        uint8_t macros[8] = {10, 20, 30, 40, 50, 60, 70, 80};
        home.setMacroValues(macros);
        home.setPatchInfo(1, "TEST PATCH", "POLY");
        
        home.render(wide_display);
        assert(wide_display.isDirty());
        wide_display.flush();

        home.render(compact_display);
        assert(compact_display.isDirty());
        compact_display.flush();

        home.render(square_display);
        assert(square_display.isDirty());
        square_display.flush();
    }

    // 2. SequencerScreen
    {
        SequencerScreen seq;
        seq.setPatternNumber(1);
        seq.setTrackMask(0, 0x1511);
        seq.setTrackPlockMask(0, 0x0010);
        
        seq.render(wide_display);
        wide_display.flush();

        seq.render(compact_display);
        compact_display.flush();

        seq.render(square_display);
        square_display.flush();
    }

    // 3. ParameterScreen
    {
        ParameterScreen param;
        param.showParameter("RESONANCE", "SYNTH A", 75.0f, 60.0f, "%", TakeoverStatus::ApproachingFromBelow);

        param.render(wide_display);
        wide_display.flush();

        param.render(compact_display);
        compact_display.flush();

        param.render(square_display);
        square_display.flush();
    }

    // 4. PadScreen
    {
        PadScreen pad;
        pad.setBankMode(PadBankMode::Drums, "808 KIT");
        pad.triggerPadHit(0, 110);

        pad.render(wide_display);
        wide_display.flush();

        pad.render(compact_display);
        compact_display.flush();

        pad.render(square_display);
        square_display.flush();
    }

    // 5. MidiMonitorScreen
    {
        MidiMonitorScreen mon;
        SynthEvent ev;
        ev.type = EventType::NoteOn;
        ev.channel = 0;
        ev.id = 60;
        ev.value = 100;
        mon.addEvent(ev);

        mon.render(wide_display);
        wide_display.flush();

        mon.render(compact_display);
        compact_display.flush();

        mon.render(square_display);
        square_display.flush();
    }

    // 6. SceneScreen
    {
        SceneScreen scene;
        scene.render(wide_display);
        wide_display.flush();

        scene.render(compact_display);
        compact_display.flush();

        scene.render(square_display);
        square_display.flush();
    }

    // 7. SplashScreen
    {
        SplashScreen splash;
        splash.onEnter();
        splash.render(wide_display);
        wide_display.flush();

        splash.render(compact_display);
        compact_display.flush();

        splash.render(square_display);
        square_display.flush();
    }

    // 8. SystemScreen
    {
        SystemScreen system;
        system.setConfigInfo("LINEAR", 54, true);
        system.render(square_display);
        square_display.flush();
    }

    // 9. MidiLearnScreen (idle state)
    {
        MidiLearnScreen learn;
        learn.render(square_display);
        square_display.flush();
    }

    assert(square_display.outOfBoundsCount() == 0);
    printf("  -> Multi-Screen rendering across all aspect ratios stayed inside display bounds.\n");
}

void testHomeHeaderSeparation() {
    printf("[TEST] Home header field separation and right-aligned MIDI/USB...\n");
    DummyDisplayDriver display(284, 76);
    assert(display.begin());
    HomeScreen home;
    home.setUsbConnected(true);
    home.setMidiActivity(true);
    home.setActiveVoices(8, 8);
    home.setBpm(300);
    home.setObservedKnobBank(2);
    home.setObservedPadBank(1);

    for (const char* name : {"A11 Brass Set 1", "MMMMMMMMMMMMMMMMMMMMMMM"}) {
        home.setPatchInfo(65535, name, "LAYERED");
        home.render(display);
        const auto* pixels = display.framebuffer();
        auto check_text_region = [&](int left, int right, uint16_t foreground) {
            bool has_text = false;
            for (int y = 3; y < 10; ++y) {
                for (int x = left; x < right; ++x) {
                    const auto pixel = pixels[y * 284 + x];
                    assert(pixel == DisplayDriver::kColorBlack || pixel == foreground);
                    has_text |= pixel == foreground;
                }
            }
            assert(has_text);
        };
        check_text_region(3, 126, DisplayDriver::kColorWhite);
        check_text_region(130, 150, DisplayDriver::kColorCyan);
        check_text_region(154, 158, DisplayDriver::kColorCyan);
        check_text_region(158, 164, DisplayDriver::kColorWhite);
        check_text_region(167, 171, DisplayDriver::kColorCyan);
        check_text_region(171, 177, DisplayDriver::kColorWhite);
        check_text_region(182, 206, DisplayDriver::kColorWhite);
        check_text_region(208, 222, DisplayDriver::kColorAmber);
        for (int left : {126, 178, 222, 258, 270}) {
            for (int y = 3; y < 10; ++y) {
                for (int x = left; x < left + 4; ++x) {
                    assert(pixels[y * 284 + x] == DisplayDriver::kColorBlack);
                }
            }
        }
        assert(pixels[3 * 284 + 262] == DisplayDriver::kColorYellow);
        assert(pixels[3 * 284 + 274] == DisplayDriver::kColorGreen);
        assert(pixels[3 * 284 + 281] == DisplayDriver::kColorGreen);
        assert(pixels[3 * 284 + 282] == DisplayDriver::kColorBlack);
        assert(pixels[3 * 284 + 283] == DisplayDriver::kColorBlack);
    }
    home.setUsbConnected(false);
    home.setMidiActivity(false);
    home.render(display);
    assert(display.framebuffer()[3 * 284 + 262] == DisplayDriver::kColorDarkGray);
    assert(display.framebuffer()[3 * 284 + 274] == DisplayDriver::kColorRed);
}

void testSquareHomeTempoAndVoicesPlacement() {
    printf("[TEST] Square Home emphasizes tempo in footer and voices in header...\n");
    BoundsCheckingDisplayDriver display(240, 240);
    assert(display.begin());
    HomeScreen home;
    home.setPatchInfo(7, "SQUARE TEST", "POLY");
    home.setBpm(128.0f);
    home.setActiveVoices(3, 8);
    home.setObservedKnobBank(1);
    home.setObservedPadBank(2);
    home.render(display);

    const auto* pixels = display.framebuffer();
    uint32_t header_cyan_pixels = 0;
    uint32_t footer_amber_pixels = 0;
    for (int y = 21; y < 30; ++y) {
        for (int x = 0; x < 240; ++x) {
            header_cyan_pixels += pixels[y * 240 + x] == DisplayDriver::kColorCyan;
        }
    }
    for (int y = 213; y < 240; ++y) {
        for (int x = 0; x < 240; ++x) {
            footer_amber_pixels += pixels[y * 240 + x] == DisplayDriver::kColorAmber;
        }
    }

    // Three active voice bars are visible in the header, while the enlarged
    // three-digit tempo creates a substantial amber footprint in the footer.
    assert(header_cyan_pixels == 3U * 4U * 4U);
    assert(footer_amber_pixels > 150);
    assert(display.outOfBoundsCount() == 0);
}

void testHomePatchNumberPrefix() {
    printf("[TEST] Home removes only the matching formatted patch-number prefix...\n");
    struct Case { uint16_t id; const char* name; const char* expected; };
    const Case cases[] = {
        {0, "000 A11 Brass Set 1    ", "000 A11 Brass Set 1"},
        {0, "A11 Brass Set 1", "000 A11 Brass Set 1"},
        {42, "042 DX7 BRASS", "042 DX7 BRASS"},
        {42, "041 DX7 BRASS", "042 041 DX7 BRASS"},
        {0, "000Lead", "000 000Lead"},
        {0, "000", "000 000"},
        {0, "00", "000 00"},
        {0, "", "000 "},
        {1000, "1000 LEAD", "1000 LEAD"},
        {65535, "65535 LEAD", "65535 LEAD"},
    };
    DummyDisplayDriver actual(284, 76), expected(284, 76);
    assert(actual.begin());
    assert(expected.begin());
    HomeScreen home;
    for (const auto& c : cases) {
        home.setPatchInfo(c.id, c.name, "SYNTH");
        home.render(actual);
        expected.fillScreen(DisplayDriver::kColorBlack);
        FontRenderer::drawString(expected, 3, 3, c.expected, DisplayDriver::kColorWhite);
        for (int y = 3; y < 10; ++y) {
            for (int x = 3; x < 126; ++x) {
                assert(actual.framebuffer()[y * 284 + x] == expected.framebuffer()[y * 284 + x]);
            }
        }
    }
}

int main() {
    printf("====================================================\n");
    printf("=== Running SMK-S3 UI Subsystem Unit Tests ===\n");
    printf("====================================================\n");

    testGeometryAndContract();
    testDirtyRectTracking();
    testFontRenderer();
    testOscilloscopeWidget();
    testWidgets();
    testMultiScreenRendering();
    testHomeHeaderSeparation();
    testSquareHomeTempoAndVoicesPlacement();
    testHomePatchNumberPrefix();

    printf("\n=== ALL UI SUBSYSTEM UNIT TESTS PASSED SUCCESSFULLY! ===\n");
    return 0;
}
