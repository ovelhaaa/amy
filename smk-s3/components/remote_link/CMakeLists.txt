idf_component_register(SRCS "remote_link.cpp" INCLUDE_DIRS "."
                       REQUIRES smk_uart audio midi synth sequencer system esp_hw_support)
