#pragma once
#include <cstdint>
#include "freertos/FreeRTOS.h"

namespace smk::config {

// ═══════════════════════════════════════════════
// Audio Configuration
// ═══════════════════════════════════════════════
constexpr uint32_t kSampleRateHz = 44100;
constexpr uint16_t kBlockSize = 256;
constexpr uint8_t  kDmaBufferCount = 3;
constexpr uint16_t kDmaBufferFrames = 256;  // frames per DMA buffer

// ═══════════════════════════════════════════════
// I²S GPIO — CONFIGURABLE PLACEHOLDERS
// These are NOT final hardware assignments.
// Change to match your actual wiring.
// ═══════════════════════════════════════════════
constexpr int kI2sBclk  = 15;
constexpr int kI2sLrclk = 16;
constexpr int kI2sData  = 17;

// ═══════════════════════════════════════════════
// USB Host
// ESP32-S3 native USB uses GPIO19 (D-) and GPIO20 (D+)
// These are fixed by silicon, not configurable.
// ═══════════════════════════════════════════════

// ═══════════════════════════════════════════════
// FreeRTOS Task Configuration
// ═══════════════════════════════════════════════
constexpr uint8_t  kAudioTaskCore     = 1;
constexpr uint8_t  kAudioTaskPriority = configMAX_PRIORITIES - 1;
constexpr uint32_t kAudioTaskStackSize = 16 * 1024;

constexpr uint8_t  kUsbHostTaskCore     = 0;
constexpr uint8_t  kUsbHostTaskPriority = configMAX_PRIORITIES - 3;
constexpr uint32_t kUsbHostTaskStackSize = 4 * 1024;

constexpr uint8_t  kMidiClientTaskCore     = 0;
constexpr uint8_t  kMidiClientTaskPriority = configMAX_PRIORITIES - 3;
constexpr uint32_t kMidiClientTaskStackSize = 4 * 1024;

constexpr uint8_t  kControlTaskCore     = 0;
constexpr uint8_t  kControlTaskPriority = configMAX_PRIORITIES - 5;

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
constexpr uint16_t kMaxOscillators = 120;
constexpr uint8_t  kDefaultPatchNumber = 0;    // Juno-6 patch 0
constexpr uint8_t  kDefaultVoiceCount = 8;

// ═══════════════════════════════════════════════
// Firmware Info
// ═══════════════════════════════════════════════
constexpr const char* kFirmwareVersion = "0.1.0";
constexpr const char* kProjectName = "SMK-S3 Synth";

} // namespace smk::config
