#pragma once
#include <cstdint>
// ROM metadata only; no AMY/LVGL dependency. Runtime residency is separate.
namespace smk::p4::library {
enum class SynthFamily : uint8_t {
    FM            = 0,
    Subtractive   = 1,
    PCM           = 2,
    KarplusStrong = 3
};

enum class PatchCategory : uint8_t {
    All           = 0,
    Piano         = 1,
    ElectricPiano = 2,
    Organ         = 3,
    Bass          = 4,
    Brass         = 5,
    Strings       = 6,
    Pads          = 7,
    Leads         = 8,
    Plucks        = 9,
    Percussion    = 10,
    FX            = 11,
    Count         = 12
};

struct Patch { uint16_t id; const char* name; uint8_t oscillators; SynthFamily family; PatchCategory category; };
inline constexpr Patch kCandidates[] = {
    {128,"DX7 BRASS 1",8,SynthFamily::FM,PatchCategory::Brass}, {135,"DX7 PIANO 1",8,SynthFamily::FM,PatchCategory::Piano},
    {138,"DX7 E.PIANO 1",8,SynthFamily::FM,PatchCategory::ElectricPiano}, {142,"DX7 BASS 1",8,SynthFamily::FM,PatchCategory::Bass},
    {4,"MOVING STRINGS",6,SynthFamily::Subtractive,PatchCategory::Strings}, {31,"SYNTH BASS I",6,SynthFamily::Subtractive,PatchCategory::Bass},
    {32,"JUNO LEAD I",6,SynthFamily::Subtractive,PatchCategory::Leads}, {8,"JUNO ORGAN I",6,SynthFamily::Subtractive,PatchCategory::Organ},
    {0,"JUNO BRASS SET 1",6,SynthFamily::Subtractive,PatchCategory::Brass}, {1,"JUNO BRASS SWELL",6,SynthFamily::Subtractive,PatchCategory::Pads},
    {12,"DONALD PLUCK",6,SynthFamily::Subtractive,PatchCategory::Plucks}, {14,"ELECT. PIANO I",6,SynthFamily::Subtractive,PatchCategory::ElectricPiano}
};
constexpr const Patch* find(uint16_t id) {
    for(const auto& patch:kCandidates) if(patch.id==id) return &patch;
    return nullptr;
}
}
