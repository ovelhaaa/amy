#pragma once
#include "amy_runtime.h"
namespace smk::p4::amy {
struct PcmMetrics {
    uint32_t peak=0,clipped=0,dsp_avg_us=0,dsp_max_us=0;
    double rms=0;
    bool release=false,panic=false;
};
// Explicit RAM-only diagnostic. Caller must yield the watchdog periodically.
// No hardware, logs or automatic musical actions from widget initialization.
bool selfTest(Runtime& runtime,PcmMetrics (&metrics)[kPatchCount],void (*yield)()=nullptr,
              uint32_t (*clock_us)()=nullptr);
}
