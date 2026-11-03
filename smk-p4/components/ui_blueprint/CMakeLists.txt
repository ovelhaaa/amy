if(CONFIG_SMKP4_UI_BLUEPRINT OR CMAKE_PROJECT_NAME STREQUAL "smk_p4_ui_blueprint")
set(blueprint_sources
        "src/ui_theme.cpp"
        "src/ui_parameters.cpp"
        "src/ui_model.cpp"
        "src/ui_demo_backend.cpp"
        "src/ui_navigation.cpp"
        "src/ui_blueprint.cpp"
        "widgets/status_strip.cpp"
        "widgets/tab_bar.cpp"
        "widgets/parameter_knob.cpp"
        "widgets/parameter_slider.cpp"
        "widgets/parameter_card.cpp"
        "widgets/patch_selector.cpp"
        "widgets/virtual_keyboard.cpp"
        "widgets/step_grid.cpp"
        "widgets/envelope_editor.cpp"
        "widgets/modulation_matrix.cpp"
        "widgets/signal_meter.cpp"
        "screens/performance_screen.cpp"
        "screens/patch_browser_screen.cpp"
        "screens/sound_editor_screen.cpp"
        "screens/layers_screen.cpp"
        "screens/mixer_screen.cpp"
        "screens/modulation_screen.cpp"
        "screens/effects_screen.cpp"
        "screens/arpeggiator_screen.cpp"
        "screens/sequencer_screen.cpp"
        "screens/drums_screen.cpp"
        "screens/harmony_screen.cpp"
        "screens/midi_screen.cpp"
        "screens/settings_screen.cpp"
        "screens/diagnostics_screen.cpp"
 )
endif()
idf_component_register(SRCS ${blueprint_sources}
    INCLUDE_DIRS "include"
    REQUIRES lvgl
)
if(blueprint_sources)
    target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
endif()
