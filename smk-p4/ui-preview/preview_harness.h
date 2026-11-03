#pragma once
#include "lvgl.h"
#include "ui_blueprint.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace smk::p4::ui::preview {

inline uint16_t g_framebuffer[kScreenWidth * kScreenHeight]{};
inline uint8_t  g_draw_buf[kScreenWidth * 40 * sizeof(uint16_t)]{};
inline uint32_t g_sim_tick_ms = 0;

inline uint32_t simTickCb() {
    return g_sim_tick_ms;
}

inline void flushToFramebuffer(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    const int32_t x1 = (area->x1 < 0) ? 0 : area->x1;
    const int32_t y1 = (area->y1 < 0) ? 0 : area->y1;
    const int32_t x2 = (area->x2 >= kScreenWidth) ? (kScreenWidth - 1) : area->x2;
    const int32_t y2 = (area->y2 >= kScreenHeight) ? (kScreenHeight - 1) : area->y2;
    const int32_t w = area->x2 - area->x1 + 1;

    const auto* src = reinterpret_cast<const uint16_t*>(px_map);
    for (int32_t y = y1; y <= y2; ++y) {
        for (int32_t x = x1; x <= x2; ++x) {
            g_framebuffer[y * kScreenWidth + x] = src[(y - area->y1) * w + (x - area->x1)];
        }
    }
    lv_display_flush_ready(disp);
}

inline lv_display_t* initHostLvgl() {
    lv_init();
    lv_tick_set_cb(simTickCb);
    lv_display_t* disp = lv_display_create(kScreenWidth, kScreenHeight);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(disp, g_draw_buf, nullptr, sizeof(g_draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flushToFramebuffer);
    return disp;
}

inline void pumpFrame(UiBlueprint& app, uint32_t step_ms = 20) {
    g_sim_tick_ms += step_ms;
    app.service(g_sim_tick_ms);
    lv_timer_handler();
    lv_refr_now(nullptr);
}

inline bool saveFramebufferPpm(const char* path) {
    FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    std::fprintf(f, "P6\n%d %d\n255\n", int(kScreenWidth), int(kScreenHeight));
    for (int32_t i = 0; i < kScreenWidth * kScreenHeight; ++i) {
        const uint16_t c = g_framebuffer[i];
        const uint8_t r5 = static_cast<uint8_t>((c >> 11) & 0x1F);
        const uint8_t g6 = static_cast<uint8_t>((c >> 5) & 0x3F);
        const uint8_t b5 = static_cast<uint8_t>(c & 0x1F);
        const uint8_t rgb[3] = {
            static_cast<uint8_t>((r5 << 3) | (r5 >> 2)),
            static_cast<uint8_t>((g6 << 2) | (g6 >> 4)),
            static_cast<uint8_t>((b5 << 3) | (b5 >> 2))
        };
        std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
    return true;
}

} // namespace smk::p4::ui::preview
