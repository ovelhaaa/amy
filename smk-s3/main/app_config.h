#pragma once
#include <cstdint>
#include "synth_config.h"
#include "audio_config.h"
#include "freertos/FreeRTOS.h"

namespace smk::config {

// ═══════════════════════════════════════════════
// Audio Configuration & PCM5102A DAC GPIOs
// ═══════════════════════════════════════════════

constexpr int kI2sBclk  = 15; // PCM5102A BCK
constexpr int kI2sLrclk = 16; // PCM5102A LCK (WS)
constexpr int kI2sData  = 17; // PCM5102A DIN
constexpr int kI2sMute  = 18; // PCM5102A SD / XSMT (High = Unmuted, Low = Muted)

// ═══════════════════════════════════════════════
// Display ST7789 SPI GPIOs & Offsets (2.25" 284x76 Widescreen)
// Bloco contíguo de pinos expostos no barramento do ESP32-S3 DevKitC
// Sem conflito com Octal PSRAM (GPIO33-37), USB Nativo (19,20), UART (43,44)
// ═══════════════════════════════════════════════
constexpr int kDisplayMosi  = 11; // SPI MOSI (SDA / DIN)
constexpr int kDisplaySclk  = 12; // SPI SCLK (SCL / CLK)
constexpr int kDisplayCs    = 10; // Chip Select (CS)
constexpr int kDisplayDc    = 9;  // Data / Command (DC / RS)
constexpr int kDisplayRst   = 8;  // Reset (RES / RST)
constexpr int kDisplayBl    = 7;  // Backlight (BLK / LED)
constexpr int16_t  kDisplayWidth   = 284;
constexpr int16_t  kDisplayHeight  = 76;
constexpr uint16_t kDisplayXOffset = 18; // ST7789 CGRAM X gap ((320 - 284) / 2)
constexpr uint16_t kDisplayYOffset = 82; // ST7789 CGRAM Y gap ((240 - 76) / 2)
constexpr bool     kDisplaySwapXy  = true;  // Landscape orientation
constexpr bool     kDisplayMirrorX = true;  // 180-degree rotation (Landscape 90 deg)
constexpr bool     kDisplayMirrorY = false; // 180-degree rotation (Landscape 90 deg)
constexpr bool     kDisplayInvertColor = false; // Fixed: Normal colors
constexpr bool     kDisplayBlActiveLow = true;  // Fixed: 2.25" panel uses negative LED backlight (active-low GND)

// ═══════════════════════════════════════════════
// USB Host
// ESP32-S3 native USB uses GPIO19 (D-) and GPIO20 (D+)
// Fixed by silicon, not configurable.
// ═══════════════════════════════════════════════

// ═══════════════════════════════════════════════
// FreeRTOS Task Configuration
// ═══════════════════════════════════════════════
constexpr uint8_t  kAudioTaskCore     = 1;
constexpr uint8_t  kAudioTaskPriority = configMAX_PRIORITIES - 1;
constexpr uint32_t kAudioTaskStackSize = 16 * 1024;
constexpr uint8_t  kSynthTaskCore = 1;
constexpr uint8_t  kSynthTaskPriority = configMAX_PRIORITIES - 2;
constexpr uint32_t kSynthTaskStackSize = 16 * 1024;

constexpr uint8_t  kUsbHostTaskCore     = 0;
constexpr uint8_t  kUsbHostTaskPriority = configMAX_PRIORITIES - 3;
constexpr uint32_t kUsbHostTaskStackSize = 4 * 1024;

constexpr uint8_t  kMidiClientTaskCore     = 0;
constexpr uint8_t  kMidiClientTaskPriority = configMAX_PRIORITIES - 3;
constexpr uint32_t kMidiClientTaskStackSize = 4 * 1024;

constexpr uint8_t  kControlTaskCore     = 0;
constexpr uint8_t  kControlTaskPriority = configMAX_PRIORITIES - 4;

constexpr uint8_t  kConsoleTaskCore     = 0;
constexpr uint8_t  kConsoleTaskPriority = 5;
constexpr uint32_t kConsoleTaskStackSize = 4 * 1024;

// ═══════════════════════════════════════════════
// Event Bus
// ═══════════════════════════════════════════════
constexpr uint16_t kEventQueueCapacity = 256;

// ═══════════════════════════════════════════════
// Synth Engine
// ═══════════════════════════════════════════════
// Synth limits and boot patches are shared via synth_config.h.

// ═══════════════════════════════════════════════
// Firmware Info
// ═══════════════════════════════════════════════
constexpr const char* kFirmwareVersion = "0.1.0";
constexpr const char* kProjectName = "SMK-S3 Synth";

} // namespace smk::config
