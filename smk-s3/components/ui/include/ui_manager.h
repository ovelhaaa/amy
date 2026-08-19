#pragma once

#include "display_driver.h"
#include "screens/home_screen.h"
#include "screens/parameter_screen.h"
#include "screens/system_screen.h"
#include "screens/midi_monitor_screen.h"
#include "screens/sequencer_screen.h"
#include "screens/pad_screen.h"
#include "synth_event.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <atomic>

namespace smk {

enum class ScreenId : uint8_t {
    Home,
    System,
    MidiMonitor,
    Sequencer,
    Pads
};

class UIManager {
public:
    UIManager(DisplayDriver& display);
    ~UIManager();

    bool begin();
    void startTask(uint8_t core_id = 0, uint8_t priority = 4);
    void stopTask();

    void switchScreen(ScreenId screen_id);
    void nextPage();
    void previousPage();
    void setPage(uint8_t page_idx);
    ScreenId activeScreenId() const { return current_screen_id_; }

    void triggerParameterOverlay(const char* name, const char* target_layer,
                                 float current_val, float saved_val, 
                                 const char* unit_str, TakeoverStatus takeover);

    void processEvent(const SynthEvent& event);

    HomeScreen& homeScreen() { return home_screen_; }
    SystemScreen& systemScreen() { return system_screen_; }
    MidiMonitorScreen& midiMonitorScreen() { return midi_monitor_screen_; }
    SequencerScreen& sequencerScreen() { return sequencer_screen_; }
    PadScreen& padScreen() { return pad_screen_; }

private:
    static void uiTaskRoutine(void* arg);

    DisplayDriver& display_;
    ScreenId current_screen_id_{ScreenId::Home};
    ScreenBase* current_screen_{nullptr};

    HomeScreen home_screen_;
    ParameterScreen parameter_screen_;
    SystemScreen system_screen_;
    MidiMonitorScreen midi_monitor_screen_;
    SequencerScreen sequencer_screen_;
    PadScreen pad_screen_;

    bool overlay_active_{false};
    TaskHandle_t task_handle_{nullptr};
    std::atomic<bool> running_{false};
};

} // namespace smk
