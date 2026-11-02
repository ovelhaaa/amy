set(AMY_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../src")

if(CONFIG_SMKP4_STANDALONE)
set(runtime_sources "amy_runtime.cpp" "self_test.cpp"
    "${AMY_SRC_DIR}/amy.c"
    "${AMY_SRC_DIR}/api.c"
    "${AMY_SRC_DIR}/algorithms.c"
    "${AMY_SRC_DIR}/oscillators.c"
    "${AMY_SRC_DIR}/filters.c"
    "${AMY_SRC_DIR}/envelope.c"
    "${AMY_SRC_DIR}/pcm.c"
    "${AMY_SRC_DIR}/patches.c"
    "${AMY_SRC_DIR}/delay.c"
    "${AMY_SRC_DIR}/instrument.c"
    "${AMY_SRC_DIR}/interp_partials.c"
    "${AMY_SRC_DIR}/log2_exp2.c"
    "${AMY_SRC_DIR}/parse.c"
    "${AMY_SRC_DIR}/sequencer.c"
    "${AMY_SRC_DIR}/custom.c"
    "${AMY_SRC_DIR}/examples.c"
    "${AMY_SRC_DIR}/midi_mappings.c"
    "${AMY_SRC_DIR}/amy_midi.c"


    "${AMY_SRC_DIR}/transfer.c"
    "${AMY_SRC_DIR}/cv_trigger.c"
)
endif()
idf_component_register(SRCS ${runtime_sources}
    INCLUDE_DIRS "." "${AMY_SRC_DIR}"
    REQUIRES esp_timer esp_psram esp_driver_uart
)

if(CONFIG_SMKP4_STANDALONE)
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    AMY_SAMPLE_RATE=48000 AMY_BLOCK_SIZE=256 BLOCK_SIZE_BITS=8
    AMY_NO_MINIAUDIO AMY_PATCH_CHECKED_RESOURCE_ADMISSION=1)
target_compile_options(${COMPONENT_LIB} PRIVATE -O2 -fno-strict-aliasing)
endif()
