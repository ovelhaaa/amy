idf_component_register(SRCS "event_bus.cpp"
                            "usb_midi_host.cpp"
                            "midi_parser.cpp"
                            "synth_event.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES freertos usb esp_timer system)
