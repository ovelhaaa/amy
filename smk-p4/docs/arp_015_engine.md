# SMK-P4 0.15 arpeggiator engine

An eight-entry USB key table captures pitch, velocity, MIDI channel and
transport generation. The fixed array preserves insertion order. A repeated
On for the same identity updates velocity and marks it held without adding
another entry. A capacity rejection increments the dropped input counter.
One bounded pattern rebuild occurs for a block with input or mode changes.
The four octave copies fit at most 32 pitches. An offset beyond MIDI 127 is
omitted.

Up sorts the full octave expansion in ascending order. Down reverses that
ordering. UpDown traverses indices 0 through N−1 and back without repeating
endpoints. As Played preserves insertion order inside each octave. Random
uses xorshift32 with seed 1 on reset and may repeat a step. Mode changes
reset traversal position. A live note-set edit reapplies the cursor modulo
the new length.

Latch Off follows physically held keys and ends the gate when the pattern
empties. Latch On retains the chord accumulated since the first press of that
gesture, including released keys until all keys are up. The first new press
after all keys are up replaces the retained chord. Turning Latch Off drops
released entries. CC64 keeps its direct-USB behavior and affects neither
arpeggiator inputs nor generated gates.

The settings are volatile. Power cycling starts with ARP disabled, tempo
120 BPM, Up, 1/16, two octaves, 65% gate, 54% swing and Latch Off.
The performance preset formats and NVS layout remain compatible.
