#pragma once

#include "display_driver.h"
#include "screens/home_screen.h"
#include "screens/parameter_screen.h"
#include "screens/system_screen.h"
#include "screens/midi_monitor_screen.h"
#include "screens/sequencer_screen.h"
#include "screens/pad_screen.h"
#include "screens/midi_learn_screen.h"
#include "screens/splash_screen.h"
#include "screens/scene_screen.h"
#include "synth_event.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <atomic>

namespace smk {

class SynthEngine;

enum class ScreenId : uint8_t {
    Splash,
    Home,
    System,
    MidiMonitor,
    Sequencer,
    Pads,
    MidiLearn,
    Scenes
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

    SplashScreen& splashScreen() { return splash_screen_; }
    HomeScreen& homeScreen() { return home_screen_; }
    SystemScreen& systemScreen() { return system_screen_; }
    MidiMonitorScreen& midiMonitorScreen() { return midi_monitor_screen_; }
    SequencerScreen& sequencerScreen() { return sequencer_screen_; }
    PadScreen& padScreen() { return pad_screen_; }
    MidiLearnScreen& midiLearnScreen() { return midi_learn_screen_; }
    SceneScreen& sceneScreen() { return scene_screen_; }

    void setWaveformSamples(const int16_t* samples, size_t count) {
        home_screen_.setWaveformSamples(samples, count);
    }
    void setSynthEngine(SynthEngine* engine) {
        synth_engine_ = engine;
    }
    DisplayDriver& display() { return display_; }

private:
    static void uiTaskRoutine(void* arg);

    DisplayDriver& display_;
    SynthEngine* synth_engine_{nullptr};
    ScreenId current_screen_id_{ScreenId::Splash};
    ScreenBase* current_screen_{nullptr};

    SplashScreen splash_screen_;
    HomeScreen home_screen_;
    ParameterScreen parameter_screen_;
    SystemScreen system_screen_;
    MidiMonitorScreen midi_monitor_screen_;
    SequencerScreen sequencer_screen_;
    PadScreen pad_screen_;
    MidiLearnScreen midi_learn_screen_;
    SceneScreen scene_screen_;

    bool overlay_active_{false};
    TaskHandle_t task_handle_{nullptr};
    std::atomic<bool> running_{false};
};

} // namespace smk
