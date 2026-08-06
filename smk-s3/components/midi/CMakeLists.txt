idf_component_register(SRCS "event_bus.cpp"
                            "usb_midi_host.cpp"
                            "midi_parser.cpp"
                            "synth_event.cpp"
                            "controller_profile.cpp"
                            "midi_learn.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES freertos usb esp_timer system)
