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

struct Patch { uint16_t id; const char* name; uint8_t oscillators; SynthFamily family; PatchCategory category; const char* bank="LEGACY"; uint8_t qualification=1; };
inline constexpr Patch kCandidates[] = {
    {128,"DX7 BRASS 1",8,SynthFamily::FM,PatchCategory::Brass}, {135,"DX7 PIANO 1",8,SynthFamily::FM,PatchCategory::Piano},
    {138,"DX7 E.PIANO 1",8,SynthFamily::FM,PatchCategory::ElectricPiano}, {142,"DX7 BASS 1",8,SynthFamily::FM,PatchCategory::Bass},
    {4,"MOVING STRINGS",6,SynthFamily::Subtractive,PatchCategory::Strings}, {31,"SYNTH BASS I",6,SynthFamily::Subtractive,PatchCategory::Bass},
    {32,"JUNO LEAD I",6,SynthFamily::Subtractive,PatchCategory::Leads}, {8,"JUNO ORGAN I",6,SynthFamily::Subtractive,PatchCategory::Organ},
    {0,"JUNO BRASS SET 1",6,SynthFamily::Subtractive,PatchCategory::Brass}, {1,"JUNO BRASS SWELL",6,SynthFamily::Subtractive,PatchCategory::Pads},
    {12,"DONALD PLUCK",6,SynthFamily::Subtractive,PatchCategory::Plucks}, {14,"ELECT. PIANO I",6,SynthFamily::Subtractive,PatchCategory::ElectricPiano},
    {131,"DX7 STRINGS 1",8,SynthFamily::FM,PatchCategory::Strings},
    {141,"DX7 SYN-LEAD 1",8,SynthFamily::FM,PatchCategory::Leads},
    {144,"DX7 E.ORGAN 1",8,SynthFamily::FM,PatchCategory::Organ},
    {151,"DX7 FLUTE 1",8,SynthFamily::FM,PatchCategory::FX}
};
enum Qualification : uint8_t { ROM_PRESENT=1, HOST_TESTED=2, SUPPORTED=4, UNAVAILABLE=8, FAILED=16, HARDWARE_TESTED=32 };
#include "factory_catalog.inc"
constexpr const Patch* factoryAt(unsigned index) { return index<256?&kFactory[index]:nullptr; }
constexpr const Patch* find(uint16_t id) {
    if(id<256) return &kFactory[id];
    for(const auto& patch:kCandidates) if(patch.id==id) return &patch;
    return nullptr;
}
}
