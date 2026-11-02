#include "display_test.h"
#include "display_hal.h"
#include "touch_hal.h"
#include "lvgl.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <cstring>

namespace smk::p4 {
static const char* TAG = "DISPLAY_TEST";
static lv_obj_t *touch_label, *button_label, *slider_label;
static lv_obj_t* link_label;
static char link_text[24]{};
void setLinkStatus(const char* text) {
    if(!link_label || !text || std::strncmp(link_text,text,sizeof(link_text))==0) return;
    std::strncpy(link_text,text,sizeof(link_text)-1); link_text[sizeof(link_text)-1]=0;
    lv_label_set_text(link_label,link_text);
}
static uint32_t presses = 0, touch_errors = 0, flush_errors = 0;
static bool touch_ready = false, display_faulted = false;
static uint64_t last_touch_us = 0;

static uint32_t tickMs() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }

static void flush(lv_display_t* display, const lv_area_t* area, uint8_t* data) {
    esp_err_t error = ESP_OK;
    if (!display_faulted) {
        error = display::bitmap(area->x1, area->y1, area->x2-area->x1+1,
                                area->y2-area->y1+1, reinterpret_cast<const uint16_t*>(data));
        if (error == ESP_OK && lv_display_flush_is_last(display)) error = display::present();
    }
    if (error != ESP_OK) {
        ++flush_errors;
        display_faulted = true;
        display::backlight(false);
        ESP_LOGE(TAG, "Display stopped: %s", esp_err_to_name(error));
    }
    lv_display_flush_ready(display);
}

static void readTouch(lv_indev_t*, lv_indev_data_t* data) {
    data->state = LV_INDEV_STATE_RELEASED;
    if (!touch_ready || display_faulted) return;
    touch::State state;
    const esp_err_t result = touch::poll(state);
    const uint64_t now_us = static_cast<uint64_t>(esp_timer_get_time());
    if (result != ESP_OK) { ++touch_errors; return; }
    if (state.updated) last_touch_us = now_us;
    if (state.pressed && state.count == 1 && now_us-last_touch_us < 300000) {
        data->point.x = state.x;
        data->point.y = state.y;
        data->state = LV_INDEV_STATE_PRESSED;
    }
    if (state.updated && touch_label)
        lv_label_set_text_fmt(touch_label, "TOUCH %s  %u,%u  RAW %u,%u",
                              state.pressed ? "DOWN" : "UP", state.x, state.y, state.raw_x, state.raw_y);
}

static lv_obj_t* label(lv_obj_t* parent, const char* text, int x, int y) {
    auto* object = lv_label_create(parent);
    lv_label_set_text(object, text);
    lv_obj_set_pos(object, x, y);
    return object;
}

bool beginDisplayTest(bool diagnostic_controls) {
    esp_err_t result = display::begin(display::Pipeline::NativeQueued);
    if (result != ESP_OK) { ESP_LOGE(TAG, "LCD init: %s", esp_err_to_name(result)); return false; }
    result = touch::begin();
    touch_ready = result == ESP_OK;
    if (!touch_ready) ESP_LOGE(TAG, "Touch init: %s; display test continues", esp_err_to_name(result));
    else ESP_LOGI(TAG, "GT%s address=0x%02x native=%ux%u", touch::product_id(), touch::address(),
                  touch::native_width(), touch::native_height());
    lv_init();
    lv_tick_set_cb(tickMs);
    auto* display = lv_display_create(800, 480);
    constexpr size_t buffer_bytes = 800u * 32u * 2u;
    void* buffer = heap_caps_aligned_alloc(64, buffer_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!display || !buffer) { ESP_LOGE(TAG, "LVGL buffer allocation failed"); display::backlight(false); return false; }
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, buffer, nullptr, buffer_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    auto* input = lv_indev_create();
    if (!input) { ESP_LOGE(TAG, "LVGL input allocation failed"); return false; }
    lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(input, readTouch);
    auto* screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101820), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(0xe6f1f5), 0);
    lv_obj_set_style_text_font(screen, &lv_font_montserrat_20, 0);
    if (!diagnostic_controls) {
        ESP_LOGI(TAG, "Performance display port ready; single touch; 800x480 RGB565 partial rendering");
        return true;
    }
    label(screen, "SMK-P4  /  GATE 1 - DISPLAY + TOUCH", 24, 24);
    label(screen, "TL 1", 12, 70); label(screen, "TR 2", 720, 70);
    label(screen, "BL 4", 12, 440); label(screen, "BR 3", 720, 440);
    link_label = label(screen, "LINK DISABLED", 245, 440);
    auto* button = lv_button_create(screen);
    lv_obj_set_pos(button, 24, 126); lv_obj_set_size(button, 340, 82);
    button_label = label(button, "BUTTON: 0", 12, 16);
    lv_obj_add_event_cb(button, [](lv_event_t*) {
        lv_label_set_text_fmt(button_label, "BUTTON: %" LV_PRIu32, ++presses);
    }, LV_EVENT_CLICKED, nullptr);
    auto* slider = lv_slider_create(screen);
    lv_obj_set_pos(slider, 50, 275); lv_obj_set_size(slider, 680, 36);
    slider_label = label(screen, "SLIDER: 0 (DISPLAY TEST ONLY)", 24, 330);
    lv_obj_add_event_cb(slider, [](lv_event_t* event) {
        lv_label_set_text_fmt(slider_label, "SLIDER: %" LV_PRId32 " (DISPLAY TEST ONLY)",
                              lv_slider_get_value(static_cast<lv_obj_t*>(lv_event_get_target(event))));
    }, LV_EVENT_VALUE_CHANGED, nullptr);
    touch_label = label(screen, touch_ready ? "TOUCH READY" : "TOUCH INIT FAILED", 24, 385);
    auto* cursor = lv_obj_create(screen);
    lv_obj_remove_style_all(cursor); lv_obj_set_size(cursor, 12, 12);
    lv_obj_set_style_radius(cursor, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(cursor, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(cursor, lv_color_hex(0xf1b84b), 0);
    lv_obj_remove_flag(cursor, LV_OBJ_FLAG_CLICKABLE);
    lv_indev_set_cursor(input, cursor);
    ESP_LOGI(TAG, "LVGL 9.3.0; 800x480 logical; partial RGB565 51200-byte PSRAM draw buffer");
    return true;
}

void serviceDisplayTest() {
    lv_timer_handler();
    static uint32_t previous_ms = 0;
    const uint32_t now_ms = tickMs();
    if (now_ms-previous_ms >= 5000) {
        previous_ms = now_ms;
        ESP_LOGI(TAG, "diag_buttons=%" LV_PRIu32 " touch_errors=%" LV_PRIu32 " flush_errors=%" LV_PRIu32
                 " refresh=%" LV_PRIu32, presses, touch_errors, flush_errors, display::refresh_count());
        lv_mem_monitor_t memory{};
        lv_mem_monitor(&memory);
        ESP_LOGI(TAG, "LVGL free=%zu largest=%zu peak_used=%zu fragmentation=%u%%",
                 memory.free_size,memory.free_biggest_size,memory.max_used,static_cast<unsigned>(memory.frag_pct));
    }
}
}
