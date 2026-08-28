#include "screens/scene_screen.h"
#include "font_renderer.h"
#include "scene_manager.h"
#include <cstdio>
#include <cstring>

namespace smk {

SceneScreen::SceneScreen() {
    const char* default_names[8] = {
        "01:INTRO",
        "02:VERSE",
        "03:PRE-CH",
        "04:CHORUS",
        "05:BRIDGE",
        "06:SOLO",
        "07:BRKDWN",
        "08:OUTRO"
    };

    for (size_t i = 0; i < 8; ++i) {
        snprintf(scene_names_[i], sizeof(scene_names_[i]), "%s", default_names[i]);
    }
}

void SceneScreen::update() {
    if (scene_mgr_) {
        active_scene_index_ = scene_mgr_->activeSceneIndex();
        pending_scene_index_ = scene_mgr_->pendingSceneIndex();

        for (uint8_t i = 0; i < 8; ++i) {
            const auto& sc = scene_mgr_->scene(i);
            snprintf(scene_names_[i], sizeof(scene_names_[i]), "%s", sc.name);
            scene_patches_[i] = sc.patch_id;
            scene_bpms_[i] = sc.bpm;
            scene_arps_[i] = sc.arp_enabled;
            scene_patterns_[i] = sc.drum_pattern;
        }
    }
}

void SceneScreen::render(DisplayDriver& display) {
    display.fillScreen(DisplayDriver::kColorBlack);
    int16_t dw = display.width();
    int16_t dh = display.height();

    // ── 160x128 (1.8" Display Layout) ──
    if (dw <= 160) {
        // Header
        char hdr[32];
        snprintf(hdr, sizeof(hdr), "SCENES [ACTIVE: #%u]", active_scene_index_ + 1);
        FontRenderer::drawString(display, 2, 2, hdr, DisplayDriver::kColorCyan, DisplayDriver::kColorBlack, FontType::Font5x7);
        display.drawHLine(0, 11, dw, DisplayDriver::kColorMidGray);

        // 8 Vertical Scene Slots (y=14..104)
        for (uint8_t i = 0; i < 8; ++i) {
            int y = 14 + i * 11;
            bool is_active = (i == active_scene_index_);
            bool is_queued = (i == pending_scene_index_);

            if (is_active) {
                display.fillRect(2, y, dw - 4, 10, DisplayDriver::kColorCyan);
                char item[32];
                snprintf(item, sizeof(item), "> %s P%02u %.0fB", scene_names_[i], scene_patches_[i], scene_bpms_[i]);
                FontRenderer::drawString(display, 4, y + 1, item, DisplayDriver::kColorBlack, DisplayDriver::kColorCyan, FontType::Font5x7);
            } else if (is_queued) {
                display.fillRect(2, y, dw - 4, 10, DisplayDriver::kColorAmber);
                char item[32];
                snprintf(item, sizeof(item), "* %s (QUEUED)", scene_names_[i]);
                FontRenderer::drawString(display, 4, y + 1, item, DisplayDriver::kColorBlack, DisplayDriver::kColorAmber, FontType::Font5x7);
            } else {
                display.drawRect(2, y, dw - 4, 10, DisplayDriver::kColorDarkGray);
                char item[32];
                snprintf(item, sizeof(item), "  %s", scene_names_[i]);
                FontRenderer::drawString(display, 4, y + 1, item, DisplayDriver::kColorLightGray, DisplayDriver::kColorBlack, FontType::Font5x7);
            }
        }

        display.drawHLine(0, 108, dw, DisplayDriver::kColorMidGray);
        FontRenderer::drawString(display, 2, 114, "PADS 1-8: SELECT SCENE", DisplayDriver::kColorYellow, DisplayDriver::kColorBlack, FontType::Font5x7);
        return;
    }

    // ── 284x76 Ultra-Widescreen Panoramic Scenes Matrix Layout ──

    // 1. Header (y=1..11)
    FontRenderer::drawString(display, 4, 2, "SCENES (PERFORMANCE)", DisplayDriver::kColorAmber, DisplayDriver::kColorBlack, FontType::Font5x7);

    char active_str[32];
    if (pending_scene_index_ >= 0) {
        snprintf(active_str, sizeof(active_str), "ACT:#%u -> CUE:#%u", active_scene_index_ + 1, pending_scene_index_ + 1);
        FontRenderer::drawString(display, 150, 2, active_str, DisplayDriver::kColorYellow, DisplayDriver::kColorBlack, FontType::Font5x7);
    } else {
        snprintf(active_str, sizeof(active_str), "ACTIVE: #%u [%s]", active_scene_index_ + 1, scene_names_[active_scene_index_]);
        FontRenderer::drawString(display, 140, 2, active_str, DisplayDriver::kColorCyan, DisplayDriver::kColorBlack, FontType::Font5x7);
    }

    display.drawHLine(0, 12, dw, DisplayDriver::kColorMidGray);

    // 2. 8-Slot Scene Performance Grid (y=14..54)
    // 2 Rows of 4 Slots (Width=67px, Height=18px, Gap=3px)
    for (uint8_t i = 0; i < 8; ++i) {
        uint8_t row = i / 4;
        uint8_t col = i % 4;
        int x = 4 + col * 70;
        int y = 14 + row * 20;
        bool is_active = (i == active_scene_index_);
        bool is_queued = (i == pending_scene_index_);

        if (is_active) {
            display.fillRect(x, y, 67, 18, DisplayDriver::kColorCyan);
            display.drawRect(x, y, 67, 18, DisplayDriver::kColorWhite);

            char label[16];
            snprintf(label, sizeof(label), ">%s", scene_names_[i]);
            FontRenderer::drawString(display, x + 3, y + 5, label, DisplayDriver::kColorBlack, DisplayDriver::kColorCyan, FontType::Font5x7);
        } else if (is_queued) {
            display.fillRect(x, y, 67, 18, DisplayDriver::kColorAmber);
            display.drawRect(x, y, 67, 18, DisplayDriver::kColorWhite);

            char label[16];
            snprintf(label, sizeof(label), "*%s", scene_names_[i]);
            FontRenderer::drawString(display, x + 3, y + 5, label, DisplayDriver::kColorBlack, DisplayDriver::kColorAmber, FontType::Font5x7);
        } else {
            display.fillRect(x, y, 67, 18, DisplayDriver::kColorBlack);
            display.drawRect(x, y, 67, 18, DisplayDriver::kColorMidGray);

            char label[16];
            snprintf(label, sizeof(label), " %s", scene_names_[i]);
            FontRenderer::drawString(display, x + 3, y + 5, label, DisplayDriver::kColorLightGray, DisplayDriver::kColorBlack, FontType::Font5x7);
        }
    }

    display.drawHLine(0, 56, dw, DisplayDriver::kColorDarkGray);

    // 3. Bottom Information & Performance Shortcuts (y=58..75)
    char det_buf[48];
    snprintf(det_buf, sizeof(det_buf), "P%02u | %.0fBPM | PAT%u | ARP:%s", 
             scene_patches_[active_scene_index_],
             scene_bpms_[active_scene_index_],
             scene_patterns_[active_scene_index_] + 1,
             scene_arps_[active_scene_index_] ? "ON" : "OFF");
    FontRenderer::drawString(display, 4, 62, det_buf, DisplayDriver::kColorAmber, DisplayDriver::kColorBlack, FontType::Font5x7);

    FontRenderer::drawString(display, 160, 62, "PADS 1-8: SELECT SCENE", DisplayDriver::kColorYellow, DisplayDriver::kColorBlack, FontType::Font5x7);
}

} // namespace smk
