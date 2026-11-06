# Harmony 0.9 short hardware smoke

Musical smoke: MANUAL / NOT RUN. Authorized0.9 application write, independent
readback and idle boot: PASS in the [physical follow-up](harmony_09_hardware_qualification.md).
Building an audible candidate does not authorize upload. Existing0.8 image,
readback logs, full-flash/app backups and rollback bundle stay untouched.

Before any write, identify the actual P4 board, port/USB identity, installed app
hash and partition table. Preserve and independently hash/read back its current
backup. Compare the concrete candidate SHA256 from the0.9 manifest. Reconfirm
existing bench electrical configuration (GPIO50 BCLK,51 WS,52 DATA, USB HS0,
VBUS/power/PCM straps as previously validated), not a new/final pin assignment.
Obtain explicit operator authorization for that exact app image and app-only
interval. Do not use an old0.8 authorization. Verify write and independent app
readback before continuing. Do not touch bootloader/partitions/NVS.

Record counter deltas and the operator's auditory observation for each item:

|Step|Action|Expected|Result|
|---|---|---|---|
|1|Boot, NORMAL KEYBOARD preset|Scale/Chord OFF, channel1, zero occupied gates|NOT RUN|
|2|Play SMK25 + Touch, Harmony OFF|Original note/velocity/FM/Juno behavior|NOT RUN|
|3|C MAJ TRIADS, play F#|Nearest-up quantizes to G; G-B-D, complete Off|NOT RUN|
|4|A MIN TRIADS, play A|A-C-E; complete Off|NOT RUN|
|5|C MAJ TRIADS, C then E|C-E-G plus E-G-B, six gates|NOT RUN|
|6|Add third triad with two slots left|Entire group rejected; existing six unchanged|NOT RUN|
|7|C MAJ SEVENTHS, C+D|Eight gates; ninth/group request rejected|NOT RUN|
|8|Hold CC64, play/release C|Whole chord sustains on that channel only|NOT RUN|
|9|Release CC64|All members release; no stuck notes|NOT RUN|
|10|Bend and CC1 with USB+Touch|All USB group members follow; Touch/other channel unchanged|NOT RUN|
|11|Choose another USB Harmony channel|Only chosen channel's future gates transformed|NOT RUN|
|12|Toggle Apply Touch|Touch future gates obey; independent Off preserved|NOT RUN|
|13|Hold C; change scale/chord/inversion|Old group frozen; new group uses new configuration|NOT RUN|
|14|Release/retrigger rapidly|New group survives old release; original velocity retained|NOT RUN|
|15|Switch128/135/138/142/4/8/31|Generation barrier, READY confirmation, no resurrected gates|NOT RUN|
|16|Navigate both Harmony tabs/tap keyboard|One gate group per press; requested/applied distinct|NOT RUN|
|17|Panic under sustain + USB + Touch|All gates/tokens/sustain canceled|NOT RUN|
|18|Disconnect/reconnect SMK25 while Touch held|USB canceled; Touch preserved; fresh USB usable|NOT RUN|
|19|Hold Touch >10 seconds|Fixed lease expires; later touches do not renew first lease|NOT RUN|
|20|30-60s sustained play, OFF then two triads|No unexpected active gaps; inspect OTHER deltas|NOT RUN|

Record: sample rate48000,256 frames, stereo PCM16, four DMA descriptors; frames,
output fallbacks by cause, stale discards, I2S errors/timeouts/refill risks, render
and compute maxima/deadlines, transform/admission maxima, group rejection/error,
USB events/drops/overflow/parse errors, internal+PSRAM free/largest, task stack,
patch and occupied gates before/after. Operator must explicitly confirm audition.
Zero I2S error is insufficient to approve gaps or latency. If unexpected gaps
occur under READY-active sustained playback, investigate that demonstrated cause
before approval. Record each row independently; do not inherit the0.8 smoke.

Prolonged qualification: PENDING, separate from this short experimental smoke.
Only after smoke PASS classify as a candidate for personal rehearsal; no stage
reliability qualification or measured latency claim. Rollback uses verified
installed0.8 app/full-flash backups after separate explicit authorization.
