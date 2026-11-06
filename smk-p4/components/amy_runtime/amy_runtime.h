#pragma once
#include "usb_input.h"
#include "output_context.h"
#include "harmony.h"
namespace smk::p4::amy {
enum class PatchState : uint8_t { Ready, Requested, Loading, Verifying, Failed };
struct Snapshot {
    harmony::Config harmony;
    uint32_t harmony_revision=0,harmony_errors=0,groups_admitted=0,groups_rejected=0;
    uint32_t last_group=0;
    uint8_t last_notes[4]{},last_count=0,last_input=0;
    bool last_admitted=false;
    bool ready=false;
    uint16_t patch_id=kPatches[0].id;
    uint8_t master=64;
    uint32_t active_notes=0, blocks=0, peak=0, clipped=0, queue_high_water=0;
    uint32_t generation=0, invalid_commands=0, expired_notes=0, command_latency_max_ms=0;
    uint32_t oom_count=0;
    uint32_t polyphony_rejected=0;
    PatchState patch_state=PatchState::Ready;
    uint16_t requested_patch_id=kPatches[0].id;
    int32_t patch_error=0;
    uint32_t patch_changes=0, patch_failures=0;
};
// All methods except commands() are called only by the owner. No I2S/LVGL here.
class Runtime {
public:
    ~Runtime() { end(); }
    static size_t bootFreeInternalBytes();
    static bool hasBootMemory(size_t free_internal_bytes,size_t largest_internal_bytes);
    bool begin(uint32_t sample_rate_hz=kSampleRateHz);
    void end();
    void apply(uint32_t now_ms);
    // Synthesis worker only; may parse/allocate. NEVER call from DMA output.
    void loadPendingPatch();
    const int16_t* render();
    Commands& commands() { return commands_; }
    UsbInput& usbInput() { return usb_; }
    const Snapshot& snapshot() const { return state_; }
private:
    void silence();
    void publishOutput(OutputReason reason) { commands_.output_context.store(outputContext(state_.generation,reason,state_.active_notes!=0),std::memory_order_release); }
    void note(unsigned slot,uint8_t velocity);
    void release(unsigned slot);
    void cancelUsb();
    unsigned findGroup(bool usb,uint8_t channel,uint8_t input) const;
    void releaseGroup(unsigned group);
    void startGroup(bool usb,const Command& c,uint32_t now_ms);
    void applyUsb(uint32_t now_ms);
    void tune(unsigned slot);
    bool loadReusable(uint16_t patch_id);
    bool verifyReusable(uint16_t patch_id);
    void failRendering();
    Commands commands_;
    UsbInput usb_;
    Snapshot state_;
    bool started_=false;
    uint8_t bank_=0;
    struct Voice {
        bool used=false, usb=false, held=false;
        uint8_t pitch=0, channel=0, input=0, group=0;
        uint32_t token=0, started_ms=0;
    } voices_[kVoices]{};
    struct Group {
        bool used=false,usb=false,held=false,sustained=false;
        uint8_t channel=0,input=0,count=0,notes[4]{},slots[4]{};
        uint16_t live=0;
        uint32_t identity=0,token=0,generation=0,started_ms=0;
        harmony::Config configuration;
    } groups_[kVoices]{};
    uint32_t next_group_=0;
    uint32_t usb_generation_=0;
    bool sustain_[16]{};
    int16_t bend_[16]{};
    uint8_t modulation_[16]{};
    uint32_t vibrato_frame_=0;
    int32_t gain_q16_=0;
#ifdef SMKP4_UNIFIED_ENGINE
    int16_t silent_pcm_[kBlockFrames*2]{};
#endif
    uint32_t load_oom_baseline_=0;
};
}
