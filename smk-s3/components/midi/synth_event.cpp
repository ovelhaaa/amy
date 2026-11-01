#include "synth_event.h"

namespace smk {
static_assert(sizeof(SynthEvent) <= 24, "SynthEvent size must be small for efficient queueing");
} // namespace smk
