# SMK-P4 0.15 touchscreen

The Arpeggiator page exposes Enable, Latch, Tempo, Rate, Direction,
Octaves, Gate, Swing, Play and Stop. Tap Tempo accepts consecutive
monotonic intervals of 250 to 1,500 ms and submits rounded BPM through
the same owner request as the tempo slider.

The live view shows internal clock, held pitches, expanded roots,
the step index, gate state and errors. It reads one coherent snapshot
published by the synthesis worker. Step refresh updates labels on the
existing view. Pending commands leave the confirmed values visible.
The software preview retains a separately labeled demo grid.

The 0.15 settings are volatile. External MIDI Clock and sequencing
remain planned features.
