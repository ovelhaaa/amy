#pragma once

#include "screen_base.h"
#include "midi_learn.h"
#include <cstdint>

namespace smk {

class MidiLearnScreen : public ScreenBase {
public:
    MidiLearnScreen();

    void setMidiLearn(MidiLearn* learn);
    void update() override;
    void render(DisplayDriver& display) override;
    const char* name() const override { return "MIDI Learn"; }

    void triggerFeedback(const char* msg, uint16_t color = DisplayDriver::kColorGreen);

private:
    MidiLearn* midi_learn_{nullptr};
    char feedback_msg_[32]{""};
    uint16_t feedback_color_{DisplayDriver::kColorGreen};
    uint32_t feedback_timer_ms_{0};
    uint8_t blink_phase_{0};
};

} // namespace smk
