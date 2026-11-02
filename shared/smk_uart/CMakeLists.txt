idf_component_register(SRCS "smk_uart.cpp" INCLUDE_DIRS "."
                       REQUIRES smk_protocol esp_driver_uart esp_driver_gpio freertos)
