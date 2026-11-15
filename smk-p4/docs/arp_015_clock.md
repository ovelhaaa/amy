# SMK-P4 0.15 musical clock

The owner advances phase by 256 frames for each synthesized block at 48 kHz.
The musical unit is a fraction of a 24 PPQN tick. A block contributes
`256 × BPM × 24 × 25` phase units. An interval at 50% swing is
`ticks × 60 × 48,000 × 25` units. The six divisions use 24, 12, 8, 6,
4 and 3 ticks in UI order.

Binary divisions alternate between twice the swing percentage and twice its
complement. A pair retains its unswung duration. Triplets stay even at every
swing setting. The clock subtracts an elapsed interval and preserves overshoot.
Every event is quantized upward to a block boundary. A fixed tempo fixture
bounds onset lateness to strictly less than 256 frames (5.333 ms) without
accumulated drift. This is a sample-count result from host. Wall-clock jitter,
DMA latency and audible timing require Guition measurements.

BPM edits change phase velocity on the next block. Rate and swing changes
select the duration of the next scheduled interval. The current gate keeps
its frame deadline. A new onset can release the prior group earlier if the
tempo increases. Off is rounded up to a block boundary, with a one-block
minimum for short gates. At maximum gate, release precedes readmission at
the next onset. Block elapsed time uses unsigned subtraction across wrap.

A stalled worker advances again on the next rendered block. The clock does
not enqueue elapsed wall-clock steps. Its phase is bounded by a single step
interval plus one block. Stop resets phase, swing parity and selection.
