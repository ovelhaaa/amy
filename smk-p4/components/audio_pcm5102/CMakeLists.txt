idf_component_register(SRCS "pcm5102_output.cpp" "pcm_test.cpp"
                       INCLUDE_DIRS "." REQUIRES esp_driver_i2s esp_driver_gpio esp_timer freertos)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
