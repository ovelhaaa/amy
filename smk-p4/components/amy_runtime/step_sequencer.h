#pragma once

#include "musical_division.h"
#include <array>
#include <algorithm>

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

struct Acknowledgement {
    uint32_t token=0;
    bool rejected=false;
};

enum class InputMode : uint8_t { Normal, StepInput, StepRecord };
constexpr uint8_t kAnyChannel=16;

struct MidiStatus {
    uint32_t revision=0, capture_revision=0, routing_errors=0;
    InputMode mode=InputMode::Normal;
    uint8_t selected=0, channel=kAnyChannel, transpose_channel=kAnyChannel;
    uint8_t gate=50, reference=60, last_note=kNoStep, last_velocity=0, captured_note=kNoStep;
    int8_t live_offset=0, effective=0;
    bool live=false, thru=false, smk_only=false, smk_connected=false;
};

struct Status {
    MidiStatus midi;
    Acknowledgement command, stop;
    uint32_t voice_drops=0;
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

    const Pattern& pattern() const { return pattern_; }
    void countVoiceDrop() { ++status_.voice_drops; }

    static constexpr bool valid(Step step) {
        return step.note<=127 && step.velocity>=1 && step.velocity<=127 &&
            step.gate>=1 && step.gate<=100;
    }

    bool setPattern(const Pattern& pattern) {
        if(!validLength(pattern.length)) return false;
        for(const auto step:pattern.steps) if(!valid(step)) return false;
        pattern_=pattern;
        status_.pattern_length=pattern.length;
        normalizeRecordStep();
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
        normalizeRecordStep();
        return true;
    }
    bool setDivision(tempo::Division division) {
        if(!tempo::divisionNumerator(division) ||
           division==tempo::Division::DottedEighth ||
           division==tempo::Division::DottedSixteenth) return false;
        // Deadlines already armed for the current step and gate stay intact.
        // update() uses this division when scheduling from the next onset.
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
        updateTranspose();
        return true;
    }
    bool select(unsigned index) {
        if(index>=kSteps) return false;
        auto& midi=status_.midi;
        midi.selected=uint8_t(index);
        normalizeRecordStep();
        ++midi.revision;
        return true;
    }
    bool moveSelection(int direction) {
        if(direction!=-1 && direction!=1) return false;
        const int length=status_.midi.mode==InputMode::StepRecord?pattern_.length:kSteps;
        return select((status_.midi.selected+length+direction)%length);
    }
    bool setInputMode(unsigned mode) {
        if(mode>unsigned(InputMode::StepRecord)) return false;
        status_.midi.mode=InputMode(mode);
        normalizeRecordStep();
        ++status_.midi.revision;
        return true;
    }
    bool setMidiChannel(unsigned channel,bool transpose) {
        if(channel>kAnyChannel) return false;
        auto& midi=status_.midi;
        if(transpose) {
            midi.transpose_channel=uint8_t(channel);
            transpose_note_=kNoStep;
            midi.live_offset=0;
            updateTranspose();
        } else midi.channel=uint8_t(channel);
        return true;
    }
    bool setRecordGate(unsigned gate) {
        if(gate<1 || gate>100) return false;
        status_.midi.gate=uint8_t(gate);
        return true;
    }
    bool setReference(unsigned note) {
        if(note>127) return false;
        auto& midi=status_.midi;
        if(transpose_note_!=kNoStep) midi.live_offset=int8_t(int(transpose_note_)-int(note));
        midi.reference=uint8_t(note);
        updateTranspose();
        return true;
    }
    void setLive(bool live) {
        status_.midi.live=live;
        transpose_note_=kNoStep;
        status_.midi.live_offset=0;
        updateTranspose();
    }
    void setSmkOnly(bool smk_only) { status_.midi.smk_only=smk_only; }
    void setSmkConnected(bool connected) { status_.midi.smk_connected=connected; }
    void setThru(bool thru) { status_.midi.thru=thru; }
    void resetMidi() {
        setLive(false);
        setInputMode(unsigned(InputMode::Normal));
        status_.midi.last_note=kNoStep;
        status_.midi.last_velocity=0;
    }
    bool midiOn(unsigned channel,uint8_t note,uint8_t velocity,bool smk25,bool& changed) {
        auto& midi=status_.midi;
        midi.last_note=note;
        midi.last_velocity=velocity;
        if(midi.smk_only && !smk25) return false;
        if(midi.live && (midi.transpose_channel==kAnyChannel || midi.transpose_channel==channel)) {
            transpose_note_=note;
            midi.live_offset=int8_t(int(note)-midi.reference);
            updateTranspose();
            return true;
        }
        if(midi.mode==InputMode::Normal || (midi.channel!=kAnyChannel && midi.channel!=channel)) return false;
        auto& step=pattern_.steps[midi.selected];
        step={note,velocity,midi.mode==InputMode::StepRecord?midi.gate:step.gate,true};
        changed=true;
        midi.captured_note=note;
        ++midi.capture_revision;
        advanceRecordStep();
        return !midi.thru;
    }
    bool rest() {
        pattern_.steps[status_.midi.selected].enabled=false;
        advanceRecordStep();
        return true;
    }
    void countRoutingError() { ++status_.midi.routing_errors; }
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

    void acknowledge(uint32_t token,bool accepted,bool stop_command=false) {
        (stop_command?status_.stop:status_.command)={token,!accepted};
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
        const int note=int(step.note)+status_.midi.effective;
        if(step.enabled && note>=0 && note<=127) {
            events.note=uint8_t(note);
            events.velocity=step.velocity;
            gate_due_=onset+interval/100*step.gate;
            gated_=true;
        }
        return events;
    }

private:
    void updateTranspose() {
        auto& midi=status_.midi;
        midi.effective=int8_t(std::clamp(int(status_.transpose)+midi.live_offset,-127,127));
    }
    void advanceRecordStep() {
        auto& midi=status_.midi;
        if(midi.mode==InputMode::StepRecord) midi.selected=uint8_t((midi.selected+1)%pattern_.length);
        ++midi.revision;
    }
    void normalizeRecordStep() {
        auto& midi=status_.midi;
        if(midi.mode==InputMode::StepRecord && midi.selected>=pattern_.length) {
            midi.selected=0;
            ++midi.revision;
        }
    }
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
    uint8_t cursor_=0,transpose_note_=kNoStep;
    bool gated_=false, odd_=false;
};

}
