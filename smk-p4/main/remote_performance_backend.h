#pragma once
#include "performance_backend.h"
#include "p4_link.h"
#include <cstring>
namespace smk::p4 {
class RemotePerformanceBackend final : public PerformanceBackend {
public:
    explicit RemotePerformanceBackend(P4Link& link):link_(link) {}
    void service(uint32_t) override {
        link::Frame reply;
        if(!connected()) { cached_=false; return; }
        if(!link_.takeReply(reply)) return;
        if(reply.type==link::Type::PatchState) {
            patch_.id=link::get16(reply.payload); patch_.available=true;
            std::memcpy(patch_.name,reply.payload+4,24); patch_.name[24]=0; cached_=true;
        } else if(reply.type==link::Type::CommandAck) { message_="S3: RECEIVED (CONTROL STATE)"; notice_=true; }
        else if(reply.type==link::Type::CommandError) { message_="S3: CONTROL REJECTED"; notice_=true; }
    }
    bool connected() const override { return link_.client().connected() && link_.hasState(); }
    bool busy() const override { return link_.client().busy(); }
    const PerformanceState& state() const override { return link_.state(); }
    uint32_t stateRevision() const override { return link_.stateRevision(); }
    const char* status() const override {
        return connected()?message_:link_.client().incompatible()?"PROTOCOL VERSION MISMATCH":link_.ready()?"WAITING FOR S3 STATE":"UART CONFIGURATION REQUIRED";
    }
    const char* takeNotice() override { if(!notice_) return nullptr; notice_=false; return message_; }
    uint16_t patchCount() const override { return 256; }
    uint16_t patchAt(uint16_t index) const override { return index; }
    bool patchInfo(uint16_t index,PatchInfo& info,uint32_t now_ms) override {
        if(cached_ && patch_.id==index) { info=patch_; return true; }
        uint8_t payload[2]; link::put16(payload,index);
        link_.client().request(link::Type::GetPatch,payload,2,now_ms); return false;
    }
    bool setPatch(uint16_t id,uint32_t now_ms) override {
        uint8_t payload[2]; link::put16(payload,id);
        return link_.client().request(link::Type::SetPatch,payload,2,now_ms);
    }
    bool setMacro(uint8_t index,uint8_t value,uint32_t now_ms) override {
        const uint8_t payload[]={index,value}; return link_.client().request(link::Type::SetMacro,payload,2,now_ms);
    }
    bool setMaster(uint8_t value,uint32_t now_ms) override { return link_.client().request(link::Type::SetMasterVolume,&value,1,now_ms); }
    bool noteOn(uint8_t note,uint8_t velocity,uint32_t now_ms) override {
        const uint8_t payload[]={note,velocity};
        return link_.client().request(link::Type::AuditionNoteOn,payload,2,now_ms);
    }
    void noteOff(uint8_t note) override { link_.client().releaseNote(note); }
    void heldNote(uint8_t note) override { link_.client().heldNote(note); }
    void panic() override { link_.client().panic(); }
private:
    P4Link& link_; PatchInfo patch_; bool cached_=false,notice_=false;
    const char* message_="S3 CONTROL STATE";
};
}
