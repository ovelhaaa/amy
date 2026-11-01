#include "synth_event.h"

namespace smk {
static_assert(sizeof(SynthEvent) <= 32, "SynthEvent size must be small for efficient queueing");
} // namespace smk
