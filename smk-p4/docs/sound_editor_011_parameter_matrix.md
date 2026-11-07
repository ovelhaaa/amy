# Parameter matrix0.11

AVAILABLE below means functional real-AMY software tests on representative
Moving Strings4 / DX7 Piano135, not hardware qualification or every ROM patch.
At runtime availability also requires a verified materialized topology. Unsupported
family/ratio mode is UNAVAILABLE; other unimplemented destinations stay PLANNED.
BUSY disables edits during load/save. Rejected commands retain confirmed values.

|Stable ID|UI ID|Family|Range|AMY destination|Active / rebuild|Test / status|
|---|---|---|---|---|---|---|
|0|filter.cutoff_hz|Juno|20..18000Hz|filter_freq_coefs CONST|active; no rebuild|PCM changes1/8 voices; AVAILABLE|
|1|filter.resonance|Juno|0.1..2Q|resonance|active; no rebuild|PCM changes1/8 voices; AVAILABLE|
|2|env.amp.attack_ms|Juno ADSR|1..8000ms API; UI to5000|eg0_times[0]|elapsed attack may change, future onset tested; no rebuild|PCM changes1/8; AVAILABLE|
|3|env.amp.release_ms|Juno ADSR|1..8000ms API; UI from5|eg0_times[2], bp_is_set|next/ongoing release slope; no rebuild|PCM release changes1/8; AVAILABLE|
|4..9|fm.op.level + selected_op1..6|FM|0..100% ROM gain|mapped operator amp CONST|active; no rebuild|all six/eight maps, OP2 PCM; AVAILABLE|
|10..15|fm.op.ratio + selected_op1..6|FM ratio mode|0.5..16|mapped ratio|active pitch/timbre; no rebuild|six mappings; OP2 PCM; AVAILABLE when ratio valid|
|16|fm.depth|FM|0..1|native modulator levels only|active; no rebuild|PCM changes, carrier retained; AVAILABLE|
|macro0|perf.macro.2|Juno/FM|0..127, neutral64|cutoff / modulation depth|active; no rebuild|manual-neutral composition LVGL test; AVAILABLE|
|macro1/2|perf.macro.5/6|Juno|0..127, neutral64|Attack/Release contributions|native envelope semantics; no rebuild|bounded owner composition; AVAILABLE|
|local|fm.selected_op|FM|1..6|presentation selection|no DSP write|LVGL/operator-map test; AVAILABLE|
|planned|filter.env_amount/keytrack|Juno|native units|filter coefficients|not enabled|PLANNED|
|planned|oscillator mix/PWM/pan; amp.level|Juno|not qualified|native coefficients|not enabled|PLANNED|
|planned|Decay/Sustain; Character|family-specific|not qualified|native envelopes/coefficients|not enabled|PLANNED|
|planned|FM feedback/algorithm, operator envelopes|FM|not qualified|ALGO/operator fields|structural changes separate|PLANNED|
|planned|LFO/modulation/FX|family-specific|not qualified|no fictitious values applied|not enabled|PLANNED|

OP levels are relative percentages of each captured operator gain, not DX7's
original integer output-level units. Fixed-frequency operators do not gain a
ratio control just because the UI has a default ratio. FM brightness explicitly
uses native carrier/modulator routing. No waveform or algorithm rewrite.
Factory filters/envelopes outside edit ranges are retained unmodified; Revert
restores captured native values, including original zero-length segments.
