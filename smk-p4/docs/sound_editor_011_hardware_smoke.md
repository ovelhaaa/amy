# Hardware smoke0.11 — MANUAL / NOT RUN

No0.11 write authorized or performed. Prior0.10 authorization does not apply.
Board reference: Guition JC-ESP32P4-M3, P4 ES1.3,16MiB Flash;
COM13 serial80:F1:B2:D3:CC:86. Bench retains GPIO50/51/52 and USB HS0;
reconfirm actual PCM power/straps/line output and Host VBUS. No new pinout.

Preflight: restore responsive serial communication, identify current firmware,
read two matching complete Flash images, preserve NVS/partition/app hashes,
compare candidate partitions, check compiled48kHz/stereo PCM16/256frames/eight
voices/64 oscillators/128KiB LVGL. Review exact firmware.bin bytes/SHA256 and
obtain operator authorization for that image. Use app-only write at0x10000,
independent readback and preserve pre-app64KiB. Do not use generic auto-upload.

Short operator smoke (no automatic30-minute requirement):

1. Boot factory; select Moving Strings, hold chord, move cutoff slowly and Q.
   Expect changing timbre with no retrigger, detuning, stuck note or UI freeze.
2. Change Attack, retrigger; change Release, release. Observe native envelope
   semantics and confirm that tails change. Compare Original/Edited then Revert.
3. Edit again, release all keys. Save As MY STRINGS01 to an empty slot.
   Expect requested/quiet/save acknowledgement, then return to playable state.
4. Select DX7 Piano. Choose OP1..6 and check levels/valid ratio modes; lower FM
   brightness. Expect audible modulation change with preserved algorithm.
   Save MY DX PIANO in another slot. Confirm overwrite requires a second press.
5. Power-cycle Guition. Open USER from Browser or Editor; recall both names,
   source IDs and values. Confirm the audible edits survived actual power loss.
6. Enable Harmony scale/chord, hold two triads, edit, release the original keys.
   All generated notes must release. Check sustain, bend/CC1, USB/Touch, Panic,
   eight voices and USB disconnect/reconnect without reboot.
7. Record EDIT011 request/revision/errors/apply_max_us, render/compute deadline,
   clipping/OOM, queue overflow/high-water, READY-active/unexpected gaps, RAM/
   largest blocks/PSRAM/stacks before/after. Separate intentional Save/Recall
   maintenance silence from unexpected gaps. DMA underruns remain unmeasured
   unless instrumented. Check cache/Flash effects during saving while silent.

Physical NVS power interruption, capacity exhaustion/GC and prolonged stability
remain separate qualification. Stop on failed save/readback, stuck notes, crash,
OOM, unexplained active gap or unstable numerical behavior. Preserve logs and
reject candidate rather than silently enabling more gains/effects.
