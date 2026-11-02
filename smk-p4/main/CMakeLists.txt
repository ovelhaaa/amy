idf_component_register(SRCS "app_main.cpp" "gate0_diagnostics.cpp" "display_test.cpp" "p4_link.cpp" "performance_ui.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES esp_system esp_hw_support spi_flash heap board lvgl esp_timer esp_psram smk_uart)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
