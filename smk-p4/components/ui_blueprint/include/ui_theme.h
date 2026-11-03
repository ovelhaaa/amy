#pragma once
#include "lvgl.h"
#include <cstdint>

namespace smk::p4::ui {

// Mandatory resolution and orientation for Guition JC-ESP32P4-M3 (ST7701S + GT911)
constexpr int32_t kScreenWidth = 800;
constexpr int32_t kScreenHeight = 480;

// Layout regions within 800x480
constexpr int32_t kStatusStripHeight = 42;
constexpr int32_t kNavBarHeight = 46;
constexpr int32_t kContentTop = kStatusStripHeight;
constexpr int32_t kContentBottom = kScreenHeight - kNavBarHeight;
constexpr int32_t kContentHeight = kContentBottom - kContentTop; // 392 px
constexpr int32_t kMargin = 8;
constexpr int32_t kMinTouchTarget = 36;

// Industrial synthesizer palette specified in SMK-P4 UI Blueprint 1.0
namespace palette {
constexpr uint32_t kBackground       = 0x101820; // Fundo principal
constexpr uint32_t kPanel            = 0x192630; // Painéis
constexpr uint32_t kPanelHighlight   = 0x253742; // Painel destacado
constexpr uint32_t kTextPrimary      = 0xE6EDF2; // Texto principal
constexpr uint32_t kTextSecondary    = 0x80919F; // Texto secundário
constexpr uint32_t kAccentPrimary    = 0xEAB65B; // Destaque principal
constexpr uint32_t kActive           = 0x67C4A5; // Ativo
constexpr uint32_t kWarning          = 0xE4A44D; // Aviso
constexpr uint32_t kErrorPanic       = 0xC94A4A; // Erro/Panic
constexpr uint32_t kBorder           = 0x354751; // Bordas
constexpr uint32_t kKeyWhite         = 0xDCE5EA; // Tecla branca
constexpr uint32_t kKeyBlack         = 0x141E26; // Tecla preta
constexpr uint32_t kPlannedBadge     = 0x3A4F5E; // Badge PLANNED
constexpr uint32_t kUnavailableBadge = 0x2A353D; // Badge UNAVAILABLE
} // namespace palette

enum class FeatureStatus : uint8_t {
    Available   = 0, // Exists and works in current P4 standalone firmware
    Planned     = 1, // Planned for future P4 standalone releases
    Unavailable = 2, // Not applicable to current patch/platform/config
    Demo        = 3  // Interactive in UI prototype, not wired to real AMY engine
};

const char* featureStatusLabel(FeatureStatus status);
uint32_t featureStatusColor(FeatureStatus status);

struct ThemeStyles {
    bool initialized = false;
    lv_style_t screen;
    lv_style_t panel;
    lv_style_t panel_highlight;
    lv_style_t button;
    lv_style_t button_active;
    lv_style_t button_accent;
    lv_style_t button_panic;
    lv_style_t tab_button;
    lv_style_t tab_button_active;
    lv_style_t badge;
    lv_style_t slider_main;
    lv_style_t slider_indicator;
    lv_style_t slider_knob;

    void init();
};

ThemeStyles& theme();

void applyScreenStyle(lv_obj_t* obj);
void applyPanelStyle(lv_obj_t* obj, bool highlighted = false);
void applyButtonStyle(lv_obj_t* obj, bool active = false, bool accent = false);
void applyPanicButtonStyle(lv_obj_t* obj);
lv_obj_t* createStatusBadge(lv_obj_t* parent, FeatureStatus status, int32_t x, int32_t y);

} // namespace smk::p4::ui
