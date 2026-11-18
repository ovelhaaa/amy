if(CONFIG_SMKP4_STANDALONE)
    set(app_sources "standalone_main.cpp" "gate0_diagnostics.cpp" "display_test.cpp" "performance_ui.cpp")
else()
    set(app_sources "app_main.cpp" "gate0_diagnostics.cpp" "display_test.cpp" "p4_link.cpp" "performance_ui.cpp")
endif()
if(CONFIG_SMKP4_UI_BLUEPRINT)
    list(APPEND app_sources "ui_blueprint_performance_adapter.cpp")
endif()
# ESP-IDF expands REQUIRES before sdkconfig is available. Keep dependencies
# unconditional; only the selected application sources initialize hardware.
idf_component_register(SRCS ${app_sources} INCLUDE_DIRS "."
                       REQUIRES esp_app_format nvs_flash esp_system esp_hw_support spi_flash heap board lvgl esp_timer esp_psram smk_uart audio_pcm5102 amy_runtime usb_midi_host ui_blueprint)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)

if(CONFIG_SMKP4_STANDALONE AND CONFIG_SMKP4_SOUND_EDITOR AND CONFIG_IDF_TARGET_ESP32P4)
    idf_component_get_property(lvgl_target lvgl__lvgl COMPONENT_LIB)
    target_include_directories(${lvgl_target} PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
    target_compile_definitions(${lvgl_target} PRIVATE
        LV_MEM_POOL_INCLUDE="lvgl_pool.h"
        LV_MEM_POOL_ALLOC=smkp4_lvgl_pool)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE SMKP4_LVGL_PSRAM_POOL=1)
endif()
