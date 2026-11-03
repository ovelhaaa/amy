if(CONFIG_SMKP4_STANDALONE)
    set(host_sources "usb_midi_host.cpp")
endif()
idf_component_register(SRCS ${host_sources} INCLUDE_DIRS "."
    REQUIRES usb freertos esp_timer amy_runtime)
# Reuse the S3's standalone, bounds-checked MIDI 1.0/FS descriptor predicate.
if(CONFIG_SMKP4_STANDALONE)
    target_include_directories(${COMPONENT_LIB} PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/../../../smk-s3/components/midi/include")
endif()
