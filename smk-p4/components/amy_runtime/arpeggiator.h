#pragma once
#include "arp_clock.h"
#include <array>

namespace smk::p4::arp {
constexpr unsigned kInputCapacity=8;
constexpr unsigned kPatternCapacity=kInputCapacity*4;

enum class Mode : uint8_t { Up, Down, UpDown, Random, AsPlayed };
enum class Action : uint16_t { Enable, Mode, Rate, Octaves, Gate, Swing, Latch, Bpm, Play, Stop, Toggle };
enum class Error : uint8_t { None, Invalid, Stale, Full };
constexpr uint8_t kProtocolVersion=1;

struct Config {
    uint8_t bpm=120, octaves=2, gate=65, swing=54;
    Mode mode=Mode::Up;
    Rate rate=Rate::Sixteenth;
    bool enabled=false, latch=false;
};

struct Confirmation {
    uint32_t token=0,generation=0;
    Action action=Action::Enable;
    uint8_t requested=0,applied=0,version=kProtocolVersion;
    Error error=Error::None;
};

struct Status {
    Config config;
    std::array<uint8_t,kInputCapacity> held{};
    std::array<uint8_t,kPatternCapacity> pattern{};
    uint8_t held_count=0, length=0, step=UINT8_MAX;
    bool armed=false, sounding=false;
    uint32_t rejected=0, revision=0;
    Confirmation touch,usb;

};

struct Input {
    uint8_t pitch=0, velocity=0, channel=0;
    uint32_t generation=0;
};

class Engine {
public:
    const Status& status() const { return status_; }
    void acknowledge(Action action,uint8_t value,uint32_t token,uint32_t generation,bool usb,Error error) {
        auto& confirmation=usb?status_.usb:status_.touch;
        confirmation={token,generation,action,value,applied(action),kProtocolVersion,error};
        ++status_.revision;
    }

    bool configure(Action action,uint8_t value) {
        auto& c=status_.config;
        switch(action) {
        case Action::Enable:
            if(value>1) return false;
            if(c.enabled!=bool(value)) { stop(); c.enabled=value; status_.armed=value; }
            break;
        case Action::Mode:
            if(value>unsigned(Mode::AsPlayed)) return false;
            if(c.mode!=Mode(value)) { c.mode=Mode(value); cursor_=0; markDirty(); }
            break;
        case Action::Rate:
            if(value>unsigned(Rate::ThirtySecond)) return false;
            c.rate=Rate(value);
            break;
        case Action::Octaves:
            if(value<1 || value>4) return false;
            c.octaves=value;markDirty();
            break;
        case Action::Gate:
            if(value<10 || value>100) return false;
            c.gate=value;
            break;
        case Action::Swing:
            if(value<50 || value>75) return false;
            c.swing=value;
            break;
        case Action::Latch:
            if(value>1) return false;
            c.latch=value;
            if(!c.latch) {
                for(unsigned i=count_; i>0; --i) if(!entries_[i-1].down) erase(i-1);
                markDirty();
            }
            break;
        case Action::Bpm:
            if(value<40 || value>240) return false;
            c.bpm=value;
            break;
        case Action::Play:
            if(c.enabled) status_.armed=true;
            break;
        case Action::Stop: stop(); break;
        case Action::Toggle: return configure(Action::Enable,!c.enabled);
        default: return false;
        }
        return true;
    }

    void stop() {
        dirty_=false;count_=0;cursor_=0;random_=1;clock_.reset();
        status_.held_count=0;status_.length=0;status_.step=UINT8_MAX;
        status_.armed=false;status_.sounding=false;
    }

    void on(Input note) {
        if(!status_.held_count && status_.config.latch) count_=0;
        for(unsigned i=0;i<count_;++i) {
            auto& entry=entries_[i];
            if(matches(entry.note,note)) {
                entry={note,true};status_.armed=true;markDirty();return;
            }
        }
        if(count_==kInputCapacity) { ++status_.rejected;return; }
        entries_[count_++]={note,true};
        status_.armed=true;markDirty();
    }

    void off(Input note) {
        for(unsigned i=0;i<count_;++i) if(matches(entries_[i].note,note)) {
            if(status_.config.latch) entries_[i].down=false;
            else erase(i);
            markDirty();return;
        }
    }

    void clearChannel(uint8_t channel) {
        for(unsigned i=count_;i>0;--i) if(entries_[i-1].note.channel==channel) erase(i-1);
        markDirty();
    }

    void prepare() {
        if(!dirty_) return;
        dirty_=false;
        rebuild();
    }

    bool next(Input& note,uint32_t& gate_frames) {
        if(!status_.length || !clock_.due()) return false;
        const auto& c=status_.config;
        gate_frames=clock_.step(c.rate,c.swing,c.bpm,c.gate);
        const unsigned length=status_.length;
        unsigned index;
        if(c.mode==Mode::Random) {
            random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;
            index=random_%length;
        } else {
            const unsigned period=c.mode==Mode::UpDown && length>1?2*(length-1):length;
            index=cursor_%period;
            cursor_=(index+1)%period;
            if(index>=length) index=period-index;
        }
        status_.step=uint8_t(index);
        const uint8_t item=pattern_[index];
        note=entries_[item%kInputCapacity].note;
        note.pitch=status_.pattern[index];
        return true;
    }

    void advance(unsigned frames) {
        if(status_.length) clock_.advance(status_.config.bpm,frames);
    }
    void sounding(bool value) { status_.sounding=value; }

private:
    struct Entry { Input note; bool down=false; };
    static bool matches(const Input& a,const Input& b) {
        return a.pitch==b.pitch && a.channel==b.channel && a.generation==b.generation;
    }
    uint8_t applied(Action action) const {
        const auto& c=status_.config;
        switch(action) {
        case Action::Enable: case Action::Toggle: return c.enabled;
        case Action::Mode: return uint8_t(c.mode);
        case Action::Rate: return uint8_t(c.rate);
        case Action::Octaves: return c.octaves;
        case Action::Gate: return c.gate;
        case Action::Swing: return c.swing;
        case Action::Latch: return c.latch;
        case Action::Bpm: return c.bpm;
        case Action::Play: case Action::Stop: return status_.armed;
        }
        return 0;
    }
    void erase(unsigned i) {
        for(;i+1<count_;++i) entries_[i]=entries_[i+1];
        --count_;
    }
    void markDirty() {
        dirty_=true;
        status_.held_count=0;
        for(unsigned i=0;i<count_;++i) if(entries_[i].down)
            status_.held[status_.held_count++]=entries_[i].note.pitch;
        if(!count_) { clock_.reset();cursor_=0; }
    }
    void rebuild() {
        status_.length=0;
        status_.step=UINT8_MAX;
        for(unsigned octave=0;octave<status_.config.octaves;++octave) {
            for(unsigned i=0;i<count_;++i) {
                const unsigned pitch=entries_[i].note.pitch+12*octave;
                if(pitch>127) continue;
                unsigned j=status_.length++;
                const Mode mode=status_.config.mode;
                while(j && mode!=Mode::AsPlayed &&
                      (mode==Mode::Down?status_.pattern[j-1]<pitch:status_.pattern[j-1]>pitch)) {
                    status_.pattern[j]=status_.pattern[j-1];pattern_[j]=pattern_[j-1];--j;
                }
                status_.pattern[j]=uint8_t(pitch);
                pattern_[j]=uint8_t(i+octave*kInputCapacity);
            }
        }
    }

    Status status_;
    Clock clock_;
    std::array<Entry,kInputCapacity> entries_{};
    std::array<uint8_t,kPatternCapacity> pattern_{};
    unsigned count_=0,cursor_=0;
    uint32_t random_=1;
    bool dirty_=false;
};
}
