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
};

} // namespace smk
