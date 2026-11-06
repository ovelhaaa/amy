# Harmony 0.9 note ownership

The synthesis worker exclusively owns an eight-entry group ledger and eight
physical voice records. A group stores source, channel, original input note,
monotonic identity, transport generation, Touch token, start time, held/sustained
state, frozen configuration, generated notes and reserved physical slots/live mask.

On admission: validate original event -> transform once -> count all needed
slots -> require complete free capacity and group entry -> reserve all slots ->
emit all AMY Ons. There is no task interleaving inside this owner operation.
Capacity shortage emits no On and increments group rejection (multi-note) or
ordinary polyphony rejection (single-note). No stealing or pitch dedup across groups.

USB identity is (USB generation, channel, original input note). Repeated On while
held is idempotent as in0.8. Re-On for a sustained group explicitly replaces that
same input group, preserving the existing retrigger policy. NoteOff/On0 looks up
the original input and releases every member, or marks every member unheld until
that channel's CC64 is released. CC120/123 immediately releases that channel.
A release clears its slot from the group live mask; the final slot frees the entry.

Touch identity is (UI generation, original input note, backend token). Release
latches are indexed by original input, never generated pitch. Same-pitch gestures
share the existing first-On ten-second lease; later gestures cannot renew it.
Old Off tokens cannot cancel a newer group. UI and owner leases are independent
safety layers. No token or monotonically exhausted group identity is wrapped.
A USB C and Touch C have separate groups/voices, including identical chords.

A group is discarded when its voices enter AMY release, as in0.8. AMY envelope
tails can continue in the physical instruments until another gate reuses them;
there is no delayed application Off that can terminate the new gate. The
musical admission count represents occupied gates, not every audible release tail.

Scale/chord/source changes affect future admissions only. Existing notes retain
their frozen original identity and all generated pitches through their release.
Patch changes invalidate UI/USB traffic before and after materialization, clear
all groups and permit gates only in READY. Global Panic clears all groups,
voices, sustain, bend/CC1 and pending generations. USB disconnect/reset/parse
recovery invalidates only USB groups and controls, preserving independent Touch.
Reconnect starts in the new USB epoch; old queued traffic is ignored.

The host matrix exercises overlap, capacity, held idempotence, retrigger, sustain,
channel/source isolation, stale tokens, lease, Panic, reconnect and patch barriers.
Hardware scheduling and auditory absence of stuck notes remain MANUAL.
