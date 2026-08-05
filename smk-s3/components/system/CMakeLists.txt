idf_component_register(SRCS "diagnostics.cpp"
                            "console.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES freertos esp_timer console esp_system esp_psram heap spi_flash fatfs vfs esp_driver_uart)
