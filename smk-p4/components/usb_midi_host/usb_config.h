#pragma once
#include <cstdint>
namespace smk::p4::usb {
constexpr unsigned kStackBytes=6144, kPriority=4, kClientMessages=8;
constexpr uint32_t kRetryMs=1000;
constexpr unsigned kMaxTransferBytes=512;
}
