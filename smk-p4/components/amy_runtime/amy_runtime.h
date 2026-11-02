#pragma once
#include "commands.h"
namespace smk::p4::amy {
struct Snapshot {
    bool ready=false;
    uint16_t patch_id=kPatches[0].id;
    uint8_t master=64;
    uint32_t active_notes=0, blocks=0, peak=0, clipped=0, queue_high_water=0;
    uint32_t generation=0, invalid_commands=0, expired_notes=0, command_latency_max_ms=0;
    uint32_t oom_count=0;
};
// All methods except commands() are called only by the owner. No I2S/LVGL here.
class Runtime {
public:
    ~Runtime() { end(); }
    bool begin(uint32_t sample_rate_hz=kSampleRateHz);
    void end();
    void apply(uint32_t now_ms);
    const int16_t* render();
    Commands& commands() { return commands_; }
    const Snapshot& snapshot() const { return state_; }
private:
    void silence();
    void note(uint8_t pitch,uint8_t velocity);
    Commands commands_;
    Snapshot state_;
    bool started_=false;
    uint8_t bank_=0;
    uint32_t tokens_[128]{},started_ms_[128]{};
    int32_t gain_q16_=0;
};
}
