# SMK-P4 unified engine0.8 — physical qualification

## Authorized app-only write and independent readback — PASS

The operator confirmed the exact candidate SHA, COM13, application-only scope,
PCM5102 GPIO50/51/52 and USB HS0. The source review bundle identifies commit
`9f79b58aa4dc359b5402e16870d352a51d6a4f1b`. The operator authorization is
recorded in `build/unified08/operator_flash_authorization.json`.

Before writing, COM13 identified ESP32-P4 v1.3, USB serial80:F1:B2:D3:CC:86.
Two independent16MiB reads matched SHA256
`5f131de44b8884e20f260587a6c0ff17508d9779d253ffba6005bb1583a44912`.
The installed application matched0.7 USB64 SHA256
`2038bb68159ecc88aa256cf286a434b32914fc5e461b320f28216691b17b26f1`;
the3072-byte partition table matched the candidate. Verified full-flash and
app-only rollback ZIP SHA256:
`97fa77f6d6f43388f9d9dc54ddac57f3825b6a7e961808b74ef849e1b20bce14`.

The esptool app-only operation wrote1211392 bytes at0x10000, erasing the
aligned application interval0x10000–0x137fff. Its own data hash check passed.
An independent read of1211392 bytes from0x10000 matched candidate SHA256
`a5b410c29b677fcdc894ee78b1f2a22c64d98594143f460edd7800a4965e8617`.
No bootloader, partition table, NVS or other flash region was requested for
writing. Raw logs: `flash_08.log`, `readback_08.log`, `boot_08.log`.

## Boot and idle observation — PASS for reported counters only

30-second serial capture after the readback reset: AMY ready; patch128 confirmed;
USB state connected; I2S producing; UI refresh advancing; notes0; render deadline
misses0, compute deadline misses0, write errors0, timeouts0, OOM0, clipping0,
USB parse errors0, dropped0 and queue overflow0. Idle render average at the
last snapshot108us and max425us. Output writer maximum blocking write5350us;
its stack high-water margin1412 bytes, synth worker4032, UI3136 and USB4144.
The output counter reported one gap before the first captured diagnostic and
no additional gaps over the observed idle snapshots; its cause is not separately
measured. Hardware DMA underruns remain UNMEASURED. No MIDI performance was
observed during this initial capture, and no audible or tactile approval follows
from these counters.

|Point|0.7 USB64 physical baseline|0.8 idle boot|
|---|---:|---:|
|Internal before AMY / largest block|387491 / 245760 B|386323 / 245760 B|
|After AMY / largest block|110259 / 36864 B|224955 / 147456 B|
|AMY initialization delta|277232 B|161368 B|
|After I2S/UI/USB / largest block|91019 / 31744 B|201263 / 126976 B|
|PSRAM free / largest after initialization|31176952 / 30932992 B|31176952 / 30932992 B|

The observed AMY initialization delta is115864 bytes lower (41.8%) and the
post-initialization internal free heap is110244 bytes higher. These are boot
points from different runs; they do not establish fragmentation behavior after
patch changes, worst-case stack, load-time deadlines, audible quality or
long-term stability. LVGL is the same128KiB static pool; do not add its unused
space to free internal heap.

## Musical smoke — MANUAL / PASS; prolonged stability — NOT RUN

The operator was asked to test Touch and SMK25, eight-note chords, FM/Juno
selection including128/135/4, release/retrigger/sustain, bend/CC1, Panic and
USB reconnect, while listening for clipping, stuck notes and clicks. The operator
replied **“Smoke passou”**. This is a manual approval of the requested smoke,
not independent instrumentation of each individual action or physical approval
of every one of the15 software candidates.

The first55-second follow-up capture contained no MIDI events or patch changes;
it was an idle observation, not the smoke. A later20-second post-smoke capture
(`post_smoke_operator_08.log`) recorded cumulative counters from the same boot:

|Metric|Post-smoke cumulative observation|
|---|---:|
|Confirmed patch changes / load failures|22 / 0|
|USB connects / disconnects|2 / 1|
|Selected-cable MIDI events / parse errors / musical queue dropped / overflow|2669 / 0 / 0 / 0|
|Ignored duplicate cable packets|2669|
|Diagnostic MIDI trace records dropped|5122|
|Render max / render deadline misses|3164us / 0|
|Synth worker command max / compute deadline misses|30995us / 22|
|Synth worker refill risks|11|
|Output fallback blocks / generation barriers|145 / 66|
|Output refill risks / I2S write errors / timeouts|0 / 0 / 0|
|Clipping / OOM / active notes at snapshot|0 / 0 / 0|
|Internal free / lowest observed / largest block|202071 / 201263 / 126976 B|
|Last swap internal before/after / largest|202071 / 202071 / 126976 B|
|Stack minimum free: synth / output / UI / USB|3492 / 1412 / 3012 / 4144 B|

The trace queue is diagnostic and distinct from the USB musical command queue:
5122 trace records were not retained, although the selected-cable command queue
reported no drops or overflows. The captured stream is therefore incomplete.
The logged compute deadline is for the synth worker and includes patch loading;
22 misses and22 selections are observed but cannot be paired individually from
this capture. The11 worker refill-risk counts are not the output-task counter,
which was0. Hardware DMA underruns remain UNMEASURED.

Output fallback counts include both generation barriers and empty/stale handoff
blocks. Of145 blocks,66 coincided with observed barrier handling and79 did not;
the latter include the one present before the first boot snapshot. At256 frames
and48kHz this represents approximately773ms of combined ramp/silence blocks
since boot, not a measurement of individual audible interruptions. The capture
cannot attribute each non-barrier block to a particular patch change or prove
that all of them were intentional. The operator's smoke PASS is preserved, but
these counters prevent an unconditional real-time/continuity PASS.

The operator chose **“Prefiro fazer depois”** when asked to perform the proposed
at-least30-minute representative FM/Juno/eight-voice/UI/MIDI window with four
instrumented hot-plugs. That window and the full physical screen/patch matrix
remain NOT RUN. The elapsed boot uptime alone does not qualify as representative
activity. Do not infer individual transition times, fragmentation stability or
audible behavior beyond the operator's reported smoke.
