#include "controller_profile.h"

#include <cassert>
#include <cstdio>

int main() {
    const smk::ControllerProfile profile = smk::ProfileManager::createDefaultSmk25Profile();

    assert(profile.knobs[0].channel == 0);
    assert(profile.knobs[15].number == 36);
    assert(profile.pads[0].channel == 9);
    assert(profile.pads[15].number == 51);
    assert(profile.buttons[0].channel == 0 && profile.buttons[0].number == 114);
    assert(profile.buttons[1].channel == 0 && profile.buttons[1].number == 115);
    assert(profile.buttons[2].channel == 0 && profile.buttons[2].number == 117);
    assert(profile.buttons[3].target_action == static_cast<uint16_t>(smk::TargetAction::Unmapped));

    assert(smk::ProfileManager::matchBinding(profile, 1, 0, 21) == smk::TargetAction::Knob1);
    assert(smk::ProfileManager::matchBinding(profile, 1, 0, 36) == smk::TargetAction::Knob16);
    assert(smk::ProfileManager::matchBinding(profile, 1, 1, 21) == smk::TargetAction::Unmapped);
    assert(smk::ProfileManager::matchBinding(profile, 1, 0, 114) == smk::TargetAction::Play);
    assert(smk::ProfileManager::matchBinding(profile, 1, 1, 114) == smk::TargetAction::Unmapped);

    assert(smk::ProfileManager::matchBinding(profile, 0, 9, 36) == smk::TargetAction::Pad1);
    assert(smk::ProfileManager::matchBinding(profile, 0, 9, 51) == smk::TargetAction::Pad16);
    assert(smk::ProfileManager::matchBinding(profile, 0, 0, 44) == smk::TargetAction::Note);

    std::puts("SMK25 controller profile tests passed.");
    return 0;
}
