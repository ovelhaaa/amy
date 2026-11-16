#pragma once

#include "musical_division.h"
#include <array>

namespace smk::p4::seq {

constexpr unsigned kSteps=16;
constexpr uint8_t kNoStep=UINT8_MAX;

struct Step {
    uint8_t note=60, velocity=100, gate=50;
    bool enabled=false;
};

struct Pattern {
    std::array<Step,kSteps> steps{};
    uint8_t length=kSteps;
};

struct Status {
    uint32_t revision=0, errors=0, token=0;
    uint16_t bpm=tempo::kDefaultBpm;
    tempo::Division division=tempo::Division::Sixteenth;
    uint8_t current_step=kNoStep, pattern_length=kSteps, swing=50;
    int8_t transpose=0;
    bool playing=false, enabled=true, quantize=false, rejected=false;
};

struct Events {
    uint8_t note=0, velocity=0; // Zero velocity means no new note.
    bool release=false;
};

class Engine {
public:
    const Status& status() const { return status_; }

    static constexpr bool valid(Step step) {
        return step.note<=127 && step.velocity>=1 && step.velocity<=127 &&
            step.gate>=1 && step.gate<=100;
    }

    bool setPattern(const Pattern& pattern) {
        if(!validLength(pattern.length)) return false;
        for(const auto step:pattern.steps) if(!valid(step)) return false;
        pattern_=pattern;
        status_.pattern_length=pattern.length;
        return true;
    }
    bool setStep(unsigned index,Step step) {
        if(index>=kSteps || !valid(step)) return false;
        pattern_.steps[index]=step;
        return true;
    }
    bool setLength(unsigned length) {
        if(!validLength(length)) return false;
        status_.pattern_length=pattern_.length=uint8_t(length);
        return true;
    }
    bool setDivision(tempo::Division division) {
        if(!tempo::divisionNumerator(division) ||
           division==tempo::Division::DottedEighth ||
           division==tempo::Division::DottedSixteenth) return false;
        status_.division=division;
        return true;
    }
    bool setSwing(unsigned swing) {
        if(swing<50 || swing>75) return false;
        status_.swing=uint8_t(swing);
        return true;
    }
    bool setTranspose(int semitones) {
        if(semitones<-24 || semitones>24) return false;
        status_.transpose=int8_t(semitones);
        return true;
    }
    void setQuantize(bool quantize) { status_.quantize=quantize; }

    bool setEnabled(bool enabled) {
        status_.enabled=enabled;
        return !enabled && stop();
    }

    bool play(const tempo::Position& position) {
        if(!status_.enabled) return false;
        if(!status_.playing) restart(position);
        return true;
    }
    bool restart(const tempo::Position& position) {
        const bool release=clearGate();
        status_.current_step=kNoStep;
        status_.playing=status_.enabled;
        cursor_=0;
        odd_=false;
        const uint64_t interval=nominalInterval();
        const auto offset=position.phase%interval;
        step_due_=position.phase+(offset ? interval-offset : 0);
        return release;
    }
    bool stop() {
        const bool release=clearGate();
        if(status_.playing) ++status_.revision;
        status_.playing=false;
        status_.current_step=kNoStep;
        return release;
    }

    void acknowledge(uint32_t token,bool accepted) {
        status_.token=token;
        status_.rejected=!accepted;
        if(!accepted) ++status_.errors;
        ++status_.revision;
    }

    Events update(const tempo::Position& position) {
        status_.bpm=position.bpm;
        Events events;
        if(!status_.playing) return events;
        if(gated_ && reached(position.phase,gate_due_)) events.release=clearGate();
        if(!reached(position.phase,step_due_)) return events;

        // Owner samples phase each render block. The shortest supported
        // swung interval is over two blocks at 240 BPM, so at most one step
        // is due per render. Preserve the scheduled onset to avoid drift.
        const auto onset=step_due_;
        const bool triplet=status_.division==tempo::Division::EighthTriplet ||
            status_.division==tempo::Division::SixteenthTriplet;
        const unsigned weight=triplet?100:2*(odd_?100-status_.swing:status_.swing);
        const uint64_t interval=nominalInterval()/100*weight;
        step_due_+=interval;
        odd_=!odd_;

        if(cursor_>=pattern_.length) cursor_=0;
        status_.current_step=cursor_;
        const auto step=pattern_.steps[cursor_++];
        ++status_.revision;
        const int note=int(step.note)+status_.transpose;
        if(step.enabled && note>=0 && note<=127) {
            events.note=uint8_t(note);
            events.velocity=step.velocity;
            gate_due_=onset+interval/100*step.gate;
            gated_=true;
        }
        return events;
    }

private:
    static constexpr bool validLength(unsigned length) { return length>=1 && length<=kSteps; }
    static constexpr bool reached(uint64_t now,uint64_t due) { return now-due<(uint64_t{1}<<63); }
    uint64_t nominalInterval() const {
        return uint64_t(tempo::kUnitsPerTick)*tempo::kPpqn*
            tempo::divisionNumerator(status_.division)/48;
    }
    bool clearGate() {
        const bool was_gated=gated_;
        gated_=false;
        return was_gated;
    }

    Pattern pattern_{};
    Status status_{};
    uint64_t step_due_=0, gate_due_=0;
    uint8_t cursor_=0;
    bool gated_=false, odd_=false;
};

}
