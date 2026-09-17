#include "ui_manager.h"
#include "dummy_display_driver.h"
#include "home_screen.h"
#include "splash_screen.h"
#include "parameter_screen.h"
#include "pad_screen.h"
#include "scene_screen.h"
#include "sequencer_screen.h"
#include <cstdio>
#include <cstdlib>

using namespace smk;

void savePPM(DummyDisplayDriver& driver, const char* filename) {
    FILE* f = fopen(filename, "wb");
    if (!f) return;
    int w = driver.width();
    int h = driver.height();
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint16_t c = driver.pixel(x, y);
            uint8_t r = ((c >> 11) & 0x1F) * 255 / 31;
            uint8_t g = ((c >> 5) & 0x3F) * 255 / 63;
            uint8_t b = (c & 0x1F) * 255 / 31;
            fwrite(&r, 1, 1, f);
            fwrite(&g, 1, 1, f);
            fwrite(&b, 1, 1, f);
        }
    }
    fclose(f);
}

int main() {
    DummyDisplayDriver driver(240, 240);
    driver.begin();

    // 1. Splash Screen
    SplashScreen splash;
    driver.fillScreen(0);
    splash.render(driver);
    savePPM(driver, "splash_screen.ppm");

    // 2. Home Screen
    HomeScreen home;
    home.setPatchInfo(12, "Deep Bass", "POLY");
    home.setHomeKnobBankView(HomeKnobBankView::BankB_Engine);
    driver.fillScreen(0);
    home.render(driver);
    savePPM(driver, "home_screen.ppm");

    // 3. Parameter Screen
    ParameterScreen param;
    param.setParameter("CUTOFF", 74, 90, "Hz");
    param.setTakeoverState(false);
    driver.fillScreen(0);
    param.render(driver);
    savePPM(driver, "parameter_takeover.ppm");

    param.setTakeoverState(true);
    driver.fillScreen(0);
    param.render(driver);
    savePPM(driver, "parameter_captured.ppm");

    // 4. Pad Screen
    PadScreen pad;
    driver.fillScreen(0);
    pad.render(driver);
    savePPM(driver, "pad_screen.ppm");

    // 5. Scene Screen
    SceneScreen scene;
    driver.fillScreen(0);
    scene.render(driver);
    savePPM(driver, "scene_screen.ppm");

    // 6. Sequencer Screen
    SequencerScreen seq;
    driver.fillScreen(0);
    seq.render(driver);
    savePPM(driver, "sequencer_screen.ppm");

    printf("Screenshots generated.\n");
    return 0;
}
