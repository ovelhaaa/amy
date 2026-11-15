# SMK-P4 0.15 architecture audit

The 0.14.2 USB client decodes cable-zero MIDI into its own SPSC command queue.
The synthesis owner checks that queue's generation before and after draining it.
Touch uses its own queue and release-token latches. The standalone worker
applies commands and then renders 256 frames at 48 kHz.

Direct USB notes identify a group by generation, channel and original pitch.
Harmony resolves that pitch to one to four outputs. The owner reserves the
whole group in its shared eight-voice ledger before any AMY On. Note Off
releases the group or marks it sustained until CC64 falls on the same channel.
Stop invalidates the USB queue and clears USB notes while Touch keeps its
own leases. Global Panic and Patch Change silence the ledger and invalidate
both producers. Patch loading repeats the barrier before accepting notes.

For 0.15, capture original USB pitches before Harmony. After current-block
command processing, service the musical clock at the render boundary. Release
the previous generated group before attempting the next admission. Give the
generated group a distinct origin and release it by its owner identity. Use
the same Harmony transformation and all-or-nothing admission used by direct
USB. Disabled arpeggiation leaves direct USB admission in place.

The S3 provides the musical division table and pattern precedents. Its timer,
EventBus and synchronization model belong to the S3 control task. The P4
will advance musical phase using rendered frames in the existing owner.

The root specification describes S3, including a wider tempo range and
external clock. This P4 candidate follows the requested 40–240 BPM internal
clock and the five requested single-root traversal modes.
