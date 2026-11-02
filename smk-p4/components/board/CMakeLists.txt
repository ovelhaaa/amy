idf_component_register(SRCS "display_hal.cpp" "touch_hal.cpp" "guition_board.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES esp_lcd esp_driver_gpio esp_driver_i2c esp_driver_ppa
                                esp_hw_support esp_timer esp_mm esp_psram)
target_compile_definitions(${COMPONENT_LIB} PRIVATE SMK_P4_DISPLAY=1)
