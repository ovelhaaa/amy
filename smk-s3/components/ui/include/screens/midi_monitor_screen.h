#pragma once

#include "screen_base.h"
#include "synth_event.h"
#include <cstdint>

namespace smk {

class MidiMonitorScreen : public ScreenBase {
public:
    static constexpr size_t kHistorySize = 4;

    MidiMonitorScreen();
    ~MidiMonitorScreen() override = default;

    void update() override;
    void render(DisplayDriver& display) override;
    const char* name() const override { return "MidiMonitor"; }

    void addEvent(const SynthEvent& event);
    void setMidiLearnActive(bool active);

private:
    struct LogEntry {
        SynthEvent event;
        bool valid{false};
    };

    LogEntry history_[kHistorySize];
    size_t head_{0};
    bool learn_active_{false};
};

} // namespace smk
