#pragma once
#include "patch_types.h"
#include <cstddef>

namespace smk {

class FactoryPatches {
public:
    static constexpr size_t kCount = 256;

    static const SynthPatch* getPatchById(uint8_t patch_id);
    static const SynthPatch* getPatchByIndex(size_t index);
    static size_t count() { return kCount; }

    // A small curated selection of existing factory presets covering both the
    // subtractive and FM families. Used to exercise the family-aware macros
    // without duplicating any patch. Returns the IDs and their count.
    static const uint8_t* showcaseIds(size_t& out_count);
};

} // namespace smk
