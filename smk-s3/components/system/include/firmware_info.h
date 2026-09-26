#pragma once

// Single source of truth for the firmware identity. Shared by the boot banner
// (main/app_config.h) and the diagnostics snapshot so the two never drift.
//
// NOTE: this is a maintained project constant, not build metadata. ESP-IDF's
// PROJECT_VER is not set for this project, so esp_app_get_description() would
// report the generic default. Automated, git-derived versioning is deliberately
// left to a later milestone.
namespace smk::config {

constexpr const char* kFirmwareVersion = "0.1.0";
constexpr const char* kProjectName = "SMK-S3 Synth";

} // namespace smk::config
