idf_component_register(
    SRCS
        "../esp_preview_main.cpp"
        "../../main/display_test.cpp"
        "../../main/gate0_diagnostics.cpp"
    INCLUDE_DIRS
        "."
        ".."
        "../../main"
    REQUIRES
        esp_system
        esp_hw_support
        spi_flash
        heap
        board
        lvgl
        esp_timer
        esp_psram
        ui_blueprint
)
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_20)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror=return-type)
