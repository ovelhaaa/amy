# Sound editor0.11 architecture

Factory ROM + manual overrides + independent macro contributions = effective
patch. Compare bypasses overrides/macros without deleting them. Revert clears
both, owner-confirmed. A saved edit remains distinguishable from factory ROM.

LVGL -> UiBlueprintPerformanceAdapter -> PerformanceBackend -> one-slot SPSC
edit mailbox -> AMY owner -> copied Snapshot -> LVGL. The UI keeps its newest
complete desired state and coalesces slider moves; at most one publish/20ms.
The musical32-command queue and independent Off/Panic latches are unchanged.
Every edit carries factory ID, generation, monotonic request ID, compact stable
parameter IDs/mask, manual values and macro positions. No strings enter AMY.

Capture occurs after native patch materialization. The owner validates all eight
instrument/voice/oscillator maps; FM resolves algo_source instead of fixed
physical offsets. UI OP1..6 reverses native OP6..1 ordering. Baseline is private
and never persisted with physical IDs or pointers. Only the owner reads synthinfo
or emits amy_event/native deltas. No AMY core modifications.

Sparse coefficient events retain all other coefficients. Partial envelopes set
bp_is_set and update existing breakpoint times only. Native logarithmic deltas
restore original cutoff/ratio exactly, avoiding lossy Hz lookup round trips.
AMY's own smoothing is retained;50Hz publication bounds control traffic.
Simple edits do not reconstruct instruments. Recall uses the existing quiescent
load/barrier/verification path then applies overrides; UI confirms after owner ACK.
A two-second edit timeout invalidates the generation through Panic and permits
retry/reconciliation; stale-generation edits are rejected. Native OOM fails closed
through the existing failed-render recovery rather than playing partial voices.

Save requests an atomic maintenance latch. The owner silences notes/sustain/groups,
invalidates generations, ACKs storage_quiet and generates zero PCM. The UI/control
task writes NVS only after that ACK; it never calls NVS from synthesis. Musical
input during the quiet interval is discarded, not replayed. Releasing the latch
creates another generation barrier. Save is rejected if the last confirmed
snapshot contains active gates or a parameter request remains pending.
NVS initialization/namespace admission precede synthesis/USB startup.

Macro composition: manual values remain intact. Neutral64 restores them.
Brightness: Juno cutoff multiplied by2^((position-64)/32), bounded20..18000Hz;
FM native modulators scaled by min(1,position/64); carriers are untouched.
The upper FM half deliberately caps at original intensity. Juno Attack/Release
multiply the manual/base time by2^((position-64)/16), bounded1..8000ms when moved.
Character, FM global envelope, mix/PWM and structural modulation remain PLANNED.
No artificial drive, tilt EQ or FX buffers; factory EQ retained, FX OFF.

UI reuses the existing seven editor tabs and widgets, with Compare/Revert/User
footer. User selection lists16 bounded slots, name and source ID; Save As also
renames/copies. Occupied slot requires a second Save As press. Removal is omitted.
Factory browser exposes the same User panel;229 admitted factory IDs and27
blocked entries are retained. Dirty factory/user switching is rejected until
Save As or Revert. Current preset name is retained in the local backend state.

Limitations: saving pauses musical admission and releases tails intentionally.
This is not seamless save during performance. Flash cache/DMA behavior during
quiet saving requires P4 hardware measurement. No perfectly continuous Compare
transition, arbitrary edited-patch clipping qualification or complete FM editor
is claimed. A silence interval may be audible during Recall/Save.
