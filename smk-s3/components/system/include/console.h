#pragma once

#include "esp_console.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace smk {

class UIManager;      // Forward declaration
class PatchManager;   // Forward declaration
class ClockManager;   // Forward declaration
class Arpeggiator;    // Forward declaration
class StepSequencer;  // Forward declaration
class StorageManager; // Forward declaration
class NvsStorage;     // Forward declaration
class MidiLearn;      // Forward declaration
class PadManager;     // Forward declaration
struct ControllerProfile; // Forward declaration

class Console {
public:
    Console();
    
    bool begin();
    void setUiManager(UIManager* ui_mgr);
    void setPatchManager(PatchManager* patch_mgr);
    void setClockManager(ClockManager* clock_mgr);
    void setArpeggiator(Arpeggiator* arp);
    void setStepSequencer(StepSequencer* seq);
    void setStorageManager(StorageManager* storage_mgr);
    void setNvsStorage(NvsStorage* nvs_storage);
    void setMidiLearn(MidiLearn* midi_learn);
    void setPadManager(PadManager* pad_mgr);
    void setActiveProfilePointer(ControllerProfile* prof_ptr);
    
    // Register additional commands
    void registerCommand(const char* name, const char* help, esp_console_cmd_func_t func);
    
private:
    static int cmdStatus(int argc, char** argv);
    static int cmdAudioStatus(int argc, char** argv);
    static int cmdPanic(int argc, char** argv);
    static int cmdMemory(int argc, char** argv);
    static int cmdMidiMonitor(int argc, char** argv);
    static int cmdReboot(int argc, char** argv);
    static int cmdUiScreen(int argc, char** argv);
    static int cmdUiParam(int argc, char** argv);
    static int cmdPatchList(int argc, char** argv);
    static int cmdPatchSelect(int argc, char** argv);
    static int cmdMacroSet(int argc, char** argv);
    static int cmdBpmSet(int argc, char** argv);
    static int cmdArpEnable(int argc, char** argv);
    static int cmdArpMode(int argc, char** argv);
    static int cmdArpSwing(int argc, char** argv);
    static int cmdSeqStep(int argc, char** argv);
    static int cmdSeqPlay(int argc, char** argv);
    static int cmdSeqStop(int argc, char** argv);
    static int cmdSeqPattern(int argc, char** argv);
    static int cmdSeqSwing(int argc, char** argv);
    static int cmdStorageInfo(int argc, char** argv);
    static int cmdPatchSave(int argc, char** argv);
    static int cmdPatchLoad(int argc, char** argv);
    static int cmdProfileSave(int argc, char** argv);
    static int cmdProfileLoad(int argc, char** argv);
    static int cmdProfileShow(int argc, char** argv);
    static int cmdLearnStart(int argc, char** argv);
    static int cmdLearnCancel(int argc, char** argv);
    static int cmdLearnSkip(int argc, char** argv);
    static int cmdKnobBank(int argc, char** argv);
    static int cmdPadBank(int argc, char** argv);
    static int cmdSceneSave(int argc, char** argv);
    static int cmdSceneLoad(int argc, char** argv);
    static int cmdPageNext(int argc, char** argv);
    static int cmdPagePrev(int argc, char** argv);
    static int cmdPageSet(int argc, char** argv);
    static int cmdPatchNext(int argc, char** argv);
    static int cmdPatchPrev(int argc, char** argv);
    static int cmdSysSave(int argc, char** argv);
    static int cmdSysLoad(int argc, char** argv);
    static int cmdHelp(int argc, char** argv);
    
    static UIManager*      s_ui_manager;
    static PatchManager*   s_patch_manager;
    static ClockManager*   s_clock_manager;
    static Arpeggiator*    s_arpeggiator;
    static StepSequencer*  s_sequencer;
    static StorageManager* s_storage_manager;
    static NvsStorage*     s_nvs_storage;
    static MidiLearn*      s_midi_learn;
    static PadManager*         s_pad_manager;
    static ControllerProfile*   s_active_profile_ptr;

    // Task for console REPL
    static void consoleTask(void* arg);
    TaskHandle_t task_handle_ = nullptr;
};

} // namespace smk
