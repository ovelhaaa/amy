#pragma once
#include "step_sequencer.h"
#include "user_presets.h"

namespace smk::p4::seq {

constexpr unsigned kPatternNameBytes=17;
constexpr unsigned kPatternRecordBytes=104;
constexpr uint32_t kPatternMagic=0x31544150;

enum class SlotState : uint8_t { Empty, Valid, Corrupted, Incompatible };
enum class ReadResult : uint8_t { Present, Missing, Incompatible, Error };

struct Slot {
    PatternSettings settings;
    char name[kPatternNameBytes]{};
    uint32_t sequence=0;
    SlotState state=SlotState::Empty;
};

inline bool validName(const char (&name)[kPatternNameBytes]) {
    if(!name[0] || name[kPatternNameBytes-1]) return false;
    for(unsigned i=0;name[i];++i)
        if(uint8_t(name[i])<32 || uint8_t(name[i])>126) return false;
    return true;
}

inline bool encodeSlot(const Slot& slot,uint8_t* bytes) {
    if(!slot.sequence || (slot.state!=SlotState::Empty &&
       (slot.state!=SlotState::Valid || !validName(slot.name) || !Engine::valid(slot.settings))))
        return false;
    std::memset(bytes,0,kPatternRecordBytes);
    edit::put32(bytes,kPatternMagic);
    edit::put32(bytes+4,1);
    edit::put32(bytes+8,slot.sequence);
    if(slot.state==SlotState::Valid) {
        const auto& settings=slot.settings;
        bytes[12]=1;
        bytes[13]=settings.pattern.length;
        bytes[14]=uint8_t(settings.division);
        bytes[15]=settings.swing;
        bytes[16]=uint8_t(int(settings.transpose)+24);
        bytes[17]=settings.quantize;
        std::memcpy(bytes+18,slot.name,kPatternNameBytes);
        for(unsigned i=0;i<kSteps;++i) {
            const auto step=settings.pattern.steps[i];
            auto* out=bytes+36+i*4;
            out[0]=step.note;out[1]=step.velocity;out[2]=step.gate;out[3]=step.enabled;
        }
    }
    edit::put32(bytes+100,edit::crc(bytes,100));
    return true;
}

inline Slot decodeSlot(const uint8_t* bytes,unsigned length) {
    Slot slot;
    slot.state=SlotState::Corrupted;
    if(length<12 || edit::get32(bytes)!=kPatternMagic) return slot;
    if(edit::get32(bytes+4)!=1) {slot.state=SlotState::Incompatible;return slot;}
    if(length!=kPatternRecordBytes || edit::get32(bytes+100)!=edit::crc(bytes,100) ||
       !edit::get32(bytes+8) || bytes[12]>1 || bytes[35]) return slot;
    if(!bytes[12]) {
        for(unsigned i=13;i<100;++i) if(bytes[i]) return slot;
        slot.state=SlotState::Empty;
    } else {
        if(bytes[16]>48 || bytes[17]>1) return slot;
        auto& s=slot.settings;
        s.pattern.length=bytes[13];s.division=tempo::Division(bytes[14]);s.swing=bytes[15];
        s.transpose=int8_t(int(bytes[16])-24);s.quantize=bytes[17];
        std::memcpy(slot.name,bytes+18,kPatternNameBytes);
        for(unsigned i=0;i<kSteps;++i) {
            const auto* in=bytes+36+i*4;
            if(in[3]>1) return slot;
            s.pattern.steps[i]={in[0],in[1],in[2],in[3]!=0};
        }
        if(!validName(slot.name) || !Engine::valid(s)) return slot;
        slot.state=SlotState::Valid;
    }
    slot.sequence=edit::get32(bytes+8);
    return slot;
}

class PatternStore {
public:
    virtual ~PatternStore()=default;

    Slot load(unsigned index) { return scan(index).slot; }

    bool commit(unsigned index,Slot next) {
        const auto current=scan(index);
        if(current.read_error || current.incompatible || current.slot.sequence==UINT32_MAX) return false;
        const unsigned bank=1-current.bank;
        next.sequence=current.slot.sequence+1;
        uint8_t bytes[kPatternRecordBytes],check[kPatternRecordBytes];
        if(!encodeSlot(next,bytes) || !write(index,bank,bytes)) return false;
        unsigned size=sizeof(check);
        return read(index,bank,check,size)==ReadResult::Present && size==sizeof(check) &&
            std::memcmp(bytes,check,size)==0;
    }

protected:
    virtual ReadResult read(unsigned index,unsigned bank,uint8_t* bytes,unsigned& length)=0;
    virtual bool write(unsigned index,unsigned bank,const uint8_t* bytes)=0;

private:
    struct Scan {
        Slot slot;
        unsigned bank=1;
        bool read_error=false,incompatible=false;
    };

    Scan scan(unsigned index) {
        Scan result;
        if(index>=kPatternSlots) {
            result.read_error=true;
            result.slot.state=SlotState::Corrupted;
            return result;
        }
        for(unsigned bank=0;bank<2;++bank) {
            uint8_t bytes[kPatternRecordBytes];
            unsigned size=sizeof(bytes);
            const auto status=read(index,bank,bytes,size);
            if(status==ReadResult::Missing) continue;
            const auto candidate=status==ReadResult::Present?decodeSlot(bytes,size):Slot{};
            result.read_error|=status==ReadResult::Error;
            result.incompatible|=status==ReadResult::Incompatible || candidate.state==SlotState::Incompatible;
            if(candidate.sequence>result.slot.sequence) {
                result.slot=candidate;
                result.bank=bank;
            } else if(!result.slot.sequence && !result.incompatible) {
                result.slot.state=SlotState::Corrupted;
            }
        }
        if(result.incompatible) result.slot.state=SlotState::Incompatible;
        return result;
    }
};

}
