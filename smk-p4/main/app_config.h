#pragma once

#include <cstdint>

namespace smk::p4::config {
constexpr uint32_t kDiagnosticsIntervalMs = 5000;
constexpr uint16_t kIntendedDisplayWidth = 800;
constexpr uint16_t kIntendedDisplayHeight = 480;
constexpr uint8_t kTouchVelocity = 100;
constexpr uint8_t kTouchMinBaseNote = 24;
constexpr uint8_t kTouchMaxBaseNote = 84;
constexpr uint32_t kTouchMaxHoldMs = 10000;
constexpr uint32_t kControlSendIntervalMs = 40;
constexpr uint32_t kPatchDebounceMs = 150;
// Confirmed display/touch profile lives in components/board/guition_board.h.
// UART uses the optional confirmed sdkconfig.defaults.link profile; P4 audio is disabled.
}
