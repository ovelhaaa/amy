#pragma once
#include "ui_navigation.h"

namespace smk::p4::ui {

// Factory functions for each of the 14 navigable screens (created on demand)
ScreenView* createPerformanceScreen();
ScreenView* createPatchBrowserScreen();
ScreenView* createSoundEditorScreen();
ScreenView* createLayersScreen();
ScreenView* createMixerScreen();
ScreenView* createModulationScreen();
ScreenView* createEffectsScreen();
ScreenView* createArpeggiatorScreen();
ScreenView* createSequencerScreen();
ScreenView* createDrumsScreen();
ScreenView* createHarmonyScreen();
ScreenView* createMidiScreen();
ScreenView* createSettingsScreen();
ScreenView* createDiagnosticsScreen();

ScreenView* createScreenById(ScreenId id);

} // namespace smk::p4::ui
