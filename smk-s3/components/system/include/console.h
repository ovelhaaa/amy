#pragma once

#include "esp_console.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace smk {

class Console {
public:
    Console();
    
    bool begin();
    
    // Register additional commands
    void registerCommand(const char* name, const char* help, esp_console_cmd_func_t func);
    
private:
    static int cmdStatus(int argc, char** argv);
    static int cmdAudioStatus(int argc, char** argv);
    static int cmdPanic(int argc, char** argv);
    static int cmdMemory(int argc, char** argv);
    static int cmdMidiMonitor(int argc, char** argv);
    static int cmdReboot(int argc, char** argv);
    static int cmdHelp(int argc, char** argv);
    
    // Task for console REPL
    static void consoleTask(void* arg);
    TaskHandle_t task_handle_ = nullptr;
};

} // namespace smk
