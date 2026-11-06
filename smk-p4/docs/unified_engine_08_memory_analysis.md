# Memory audit 0.8

Baseline command: `./smk-p4/tests/run_amy.ps1 -Voices 8 -SoundLibrary` PASS
(real engine, host GCC, source bd564872). Raw output: `build/baseline08.log`.
Host allocation wrappers measure requested live bytes, not ESP heap capacity,
largest free block, allocator overhead or fragmentation. They exclude static
DRAM, task stacks, USB, DMA, LVGL and framebuffers.

|Host point|Live requested bytes|Resident instruments / allocated oscs|
|---|---:|---:|
|Probe four FM banks after warmup|237338|32 / 256|
|Probe one FM bank after warmup|116618|8 / 64|
|Probe one Juno bank after warmup|107594|8 / 48|
|Extra FM bank|38976 incremental|8 / 64 incremental|
|Extra Juno bank|29952 incremental|8 / 48 incremental|

Same-cost replacement of eight instruments made 24 tracked allocations (object
and two FIFOs per instrument). Engine startup/pool reservation is additional;
the isolated bank probe is not the full Runtime budget. Reuse must account for
delta pool high-water retention and lazy oscillator/filter allocations.

Historical P4 USB64 points from `sound_library_07_hardware_qualification.md`:
before engine 387491 free internal / 245760 largest; after engine 110259 / 36864;
after I2S/UI/connected USB 91019 / 31744. Observed engine delta 277232 bytes.
These are preserved baseline observations, NOT new runs. PSRAM approximately
31 MB and LVGL peak near 50 KB are reported historical approximations, not a
fresh synchronized snapshot. No new hardware capture or reset was performed.

Allocation domains must remain separate:

- AMY synth/instruments/oscillator state, events/deltas, PCM block and mix buffers
  request internal 8-bit heap. Runtime limits define pool capacity; instruments
  and DSP state materialize separately. Delta blocks persist at high-water.
- Delay/sample/sysex caps target PSRAM; disabled FX do not grant the same savings
  as moving hot state. ROM patch strings add flash/catalog metadata, not voices.
- Audio owner stack is 12288 bytes. USB/UI stacks and USB endpoint allocations
  belong to their own tasks/drivers; inspect task high-water and capability heap
  before/after initialization, connection, each swap and navigation on target.
- I2S DMA descriptors/buffers belong to the driver and require suitable memory;
  host wrappers do not include these allocations.
- Display HAL requests two/three external framebuffers; FullPpa also allocates a
  logical PSRAM buffer. UI allocation pool is separately reserved; see LVGL audit.
- Static DRAM requires ELF/map section sizes, not subtraction of heap readings.

New A/B/C measurements and exact limitations are appended after isolated reuse
stress. Target largest blocks, fragmentation, stack and total memory during
swaps remain MANUAL until an explicitly authorized image is exercised.

## Isolated reuse experiment (PASS)

`./smk-p4/tests/run_unified_probe.ps1`, `build/unified08/probe.log`: 2005
selections per architecture across128/135/138/142/4. One eight-voice instrument:
114042 live bytes,140 allocations alive; eight one-voice instruments:116618
bytes,161 alive. After warmup every same-patch live-byte reading was identical.
All teardown readings returned to baseline. Whole-set deletion before load
avoids tight-pool six/eight-osc map fragmentation. Oscillator storage remains
at its FM high-water even on Juno;48 assigned oscs does not imply48 allocated
objects. Replacement after warmup makes3/24 allocations respectively.

Observed host maximum load+materialization:1362/1413 microseconds respectively;
these are scheduler-sensitive host observations, not target deadlines. Chord
PCM was nonzero and unclipped for this fixed five-patch scenario; Panic output
was exactly zero. Guarded steady render made no heap calls. Host allocator
fragmentation/largest free block is NOT measured by requested-byte equality.

Choose eight reusable instruments:2576 bytes extra versus one, preserving P4
control semantics. Configured capacity becomes64 oscillators/eight AMY voices,
independent of catalog count. Do not compare isolated116618 bytes directly
with full-runtime286490 or target277232: delta reservations differ.
