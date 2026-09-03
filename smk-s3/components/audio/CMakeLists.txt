idf_component_register(SRCS "pcm5102_output.cpp" "amy_adapter.cpp" "test_tone.cpp" "audio_task.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES amy_engine esp_driver_i2s freertos midi system)
