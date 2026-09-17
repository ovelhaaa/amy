#pragma once

#include <cstdint>

namespace smk {
namespace theme {

// ═══════════════════════════════════════════════════════════════════════════
// COLOR PALETTE (RGB565)
// ═══════════════════════════════════════════════════════════════════════════

// Canvas (Backgrounds & Surfaces)
constexpr uint16_t ColorBackground     = 0x0862; // #0B0E12
constexpr uint16_t ColorSurface        = 0x10C4; // #12171D
constexpr uint16_t ColorSurfaceElev    = 0x18E4; // #181E25
constexpr uint16_t ColorDivider        = 0x2987; // #29313A

// Text
constexpr uint16_t ColorTextPrimary    = 0xF7BE; // #F3F5F7
constexpr uint16_t ColorTextSecondary  = 0x9515; // #98A2AD
constexpr uint16_t ColorTextMuted      = 0x5B2E; // #596571

// Accents
constexpr uint16_t ColorAccentPrimary  = 0x3EB8; // #37D7C4 (Aqua/Teal)
constexpr uint16_t ColorAccentSecondary= 0xEDA9; // #F2B84B (Amber)

// States
constexpr uint16_t ColorStatePlay      = 0x360D; // #33C471 (Moderate Green)
constexpr uint16_t ColorStateRecord    = 0xDA49; // #E24C4B (Coral Red)
constexpr uint16_t ColorStateWarning   = ColorAccentSecondary;
constexpr uint16_t ColorStateFocus     = ColorAccentPrimary;
constexpr uint16_t ColorStateMuted     = 0x632C; // Darkened red-ish

// Semantic Roles
constexpr uint16_t ColorBankA          = ColorAccentPrimary;
constexpr uint16_t ColorBankB          = ColorAccentSecondary;
constexpr uint16_t ColorHeader         = ColorSurface;

// ═══════════════════════════════════════════════════════════════════════════
// SPATIAL TOKENS (Base 4px grid for 240x240)
// ═══════════════════════════════════════════════════════════════════════════

constexpr int16_t kDisplayWidth        = 240;
constexpr int16_t kDisplayHeight       = 240;

constexpr int16_t kMargin              = 8;
constexpr int16_t kPadding             = 4;

constexpr int16_t kHeaderHeight        = 26;

constexpr int16_t kCornerRadius        = 4;

// ═══════════════════════════════════════════════════════════════════════════
// MACRO TILE LAYOUT
// ═══════════════════════════════════════════════════════════════════════════

// For a 4x2 grid in 240x240
constexpr int16_t kMacroTileWidth      = 54;
constexpr int16_t kMacroTileHeight     = 42;
constexpr int16_t kMacroTileGapX       = 2;
constexpr int16_t kMacroTileGapY       = 2;
constexpr int16_t kMacroStartX         = 9;

} // namespace theme
} // namespace smk
