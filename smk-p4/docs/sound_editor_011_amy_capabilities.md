# AMY/S3 capability audit — 0.11

Baseline: dfcef41dd04eaa034fc4c20945bfd95ce65062d9 (factory0.10).
Explicit P4/800x480 request supersedes specs.md S3/284x76 only for this isolated
application. No dependency upgrade, ROM edit or AMY core change is required.

## Native API and ownership

Inspected src/amy.h, amy.c, oscillators.c, patches.c/h, instrument.c,
algorithms.c, envelope.c, docs/api.md and docs/synth.md. amy_event fields are
sparse: unset coefficients leave other terms unchanged. Frequency coefficient0
is Hz at the event boundary and logarithmic in synthinfo. Ratio is log2 internally.
Envelope event times are milliseconds, materialized times are samples. The
allocated breakpoint capacity is not its length: scan the unset sentinel.
Updating one breakpoint does not truncate the remaining envelope in this AMY.

The P4 uses eight reusable independent instruments. voice_to_base_osc and
osc_to_voice must be checked for each instrument; physical indices are not preset
identities. ALGO algo_source runs in DX7 OP6..OP1 order. Validate source ownership
before mapping a UI OP1..6 selection. algorithms.c routes operators with output
bus bits1/2 to modulation buses; remaining outputs are carriers. Reading that
existing const table avoids inventing carrier maps. Structural routing stays ROM.

|Parameter|Native event|Units / conservative edit range|Family|Update / active notes|Allocation / reconstruction|
|---|---|---|---|---|---|
|Cutoff|filter_freq_coefs[COEF_CONST]|20..18000 Hz|Juno|sparse scalar; active filter follows native smoothing|bounded deltas; no new oscillator/filter topology|
|Resonance|resonance|0.1..2 Q|Juno|active notes; preserves topology|bounded deltas; none|
|Attack|eg0_times[0]|1..8000 ms|Juno ADSR|native timing; elapsed attack may change immediately; strongest effect next onset|existing breakpoint only; no vector growth|
|Release|eg0_times[2]|1..8000 ms|Juno ADSR|next release; changing an ongoing release may change its slope|existing breakpoint only; no vector growth|
|Operator level|amp_coefs[COEF_CONST]|0..100% of captured ROM gain|FM|all eight corresponding operators; carriers affect level, modulators timbre|bounded deltas; none|
|Operator ratio|ratio|0.5..16, only valid materialized ratios|FM ratio-mode ops|active pitch/timbre changes without velocity trigger|none; fixed-frequency ops unavailable|
|FM brightness|operator amplitude events|0..1 of manual modulator gains|FM|native routing classifies modulators, carriers retained|none|
|Filter env/key tracking|filter_freq_coefs[COEF_EG0/NOTE]|native coefficient units|Juno|possible sparse update; planned|not enabled in0.11 initial subset|
|Mix/PWM/pan|amp/duty/pan coefficients|family-specific|Juno|planned; must preserve modulation coefficients|no speculative mapping enabled|
|Decay/sustain|eg0_times[1]/eg0_values[1]|ms / amplitude|Juno|planned|existing vectors possible, not qualified here|
|Waveform/algorithm|wave/algorithm/algo_source|structural|family-specific|not enabled; quiescent reconstruction required|may allocate DSP/oscillator resources|
|LFO/mod routing|mod_source, coefficient vectors|family-specific|both|planned; cannot blindly retarget global osc0|may allocate source; separate qualification|
|Instrument gain|synth_level|linear|both|native local gain; planned|distinct from global volume/EQ|
|FX/EQ|bus events|global bus|both|chorus/echo/reverb OFF; factory EQ preserved|no new buffers|

## S3 reference and differences

Inspected patch_types.h, macro_profile.h, patch_manager.cpp and amy_adapter.cpp
(including executeFilter, executeEnvelope and FM baseline capture). S3
ManualControlState separates manual values from macro contributions. Preserve
that composition principle, not its large patch manager/persistent ABI. S3
executeEnvelope writes amplitude coefficients and complete ADSR: copying it
blindly would replace the P4 ROM modulation semantics. P4 updates only existing
times, retains decay/sustain/amp coefficients and uses its own eight-voice map.
S3 formats have versions/CRC; P4 needs explicit byte serialization rather than
copying padded C++ structs or physical operator addresses.

No new dependency. Application-only native-table declaration can be replaced by
an upstream public carrier-query API later; no upstream patch is necessary now.
Functional test evidence and remaining limits live in sound_editor_011_results.md.
