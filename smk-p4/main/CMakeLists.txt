if(CONFIG_SMKP4_STANDALONE)
    set(app_sources "standalone_main.cpp" "gate0_diagnostics.cpp" "display_test.cpp" "performance_ui.cpp")
else()
    set(app_sources "app_main.cpp" "gate0_diagnostics.cpp" "display_test.cpp" "p4_link.cpp" "performance_ui.cpp")
endif()
# ESP-IDF expands REQUIRES before sdkconfig is available. Keep dependencies
# unconditional; only the selected application sources initialize hardware.
idf_component_register(SRCS ${app_sources} INCLUDE_DIRS "."
                       REQUIRES esp_system esp_hw_support spi_flash heap board lvgl esp_timer esp_psram smk_uart audio_pcm5102 amy_runtime usb_midi_host)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
