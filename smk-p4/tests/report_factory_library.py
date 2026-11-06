"""Build concise audio/memory notes from executed qualification evidence."""
import collections
import json
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent))
from audit_patch_library import ROOT,inventory
from generate_factory_library import category

def main():
    docs=ROOT/'smk-p4/docs';result=json.loads((docs/'factory_library_010_metrics.json').read_text())
    patches=result['patches'];_,_,rom=inventory()
    admitted=[p for p in patches if p['supported']]
    categories=collections.Counter(category(rom[p['id']][0]) for p in admitted)
    soft=[(p['id'],sum(t['observer']['soft'] for t in p['tests'])) for p in admitted if any(t['observer']['soft'] for t in p['tests'])]
    observed_max=max(t['max_us'] for p in patches for t in p['tests'])
    retest_max=max(t['max_us'] for p in patches for t in p['timing_retest'])
    audio=f'''# Factory library 0.10 — audio analysis

PASS: actual AMY fixed-point render,48kHz/stereo PCM16,256 frames,FX OFF,ROM EQ retained. Tested256 patches with1/4/8 notes across MIDI36/48/60/72 roots plus three-semitone chord intervals and velocities48/100/127. Each attack window384 blocks (~2.048s). Final high-register velocity127 releases are observed after15000 blocks (80s), then eight more blocks; intervening scenarios use256 release blocks. Silence is evaluated over the whole musical matrix, never C4 alone.

Result: {len(admitted)} experimental host candidates;24 patches fail hard-clipping criteria and three exceed the release-tail threshold. Detailed per-ID reasons and1/4/8 results are in the generated matrix/JSON. None failed structural maps, eight-voice materialization, OOM, guarded render allocations, Panic or balanced engine teardown. Unsupported IDs remain blocked; a quality rejection is not hidden by the application master.

The production AMY core is unchanged. A generated build-only copy inserts an observer immediately before AMY's final soft/hard conversion, using the pinned29491 soft-knee and34405 hard-clip thresholds. This separates AMY's final nonlinear conversion from the later P4 application master. The observer and unmodified render are separate takes: releases, global sample clocks/LFO phases and noise differ, so their peak counts are not sample-aligned or ratios of the same waveform. Fixed-point integer PCM has no NaN/Inf representation. No exhaustive proof of absence of intermediate oscillator/filter arithmetic saturation or wrap is claimed.

## Patch32 — Juno A51 Lead I

Factory commands contain pulse amplitude1, saw/sub/noise amplitude0, parent amplitude0.156, resonance5.818, cutoff coefficients216.7/0.409/.../3.118, and EQ+7/-3/-3dB. Single and four-note unmodified PCM did not saturate; eight-note PCM reached32767 with34 saturated samples in the tested matrix. The separate observer reached64881 before conversion, with2122 soft-knee and532 hard-clip samples at eight notes; one/four-note observer peaks7412/23877 had no clipping. This demonstrates a polyphonic-sum/final-AMY-conversion overload before the application master. Resonant/filter and EQ gain contribute plausible amplification; it does not establish that every internal arithmetic stage is free of saturation. It is reproducible with the real host AMY path and is not solely the P4 PCM5102A conversion. Patch32 stays blocked. No global/per-patch gain change or DSP algorithm edit was made.

Release-review patches69/112 ended at up to5 raw LSB after80s; patch254 at2587 LSB. These are observed tails, not proof of held-note ownership failure. They remain unavailable pending analysis of intended envelopes; Panic clears PCM and runtime held groups. The threshold is conservative, not an assertion that all long tails are defective.

Admitted patches with a small intentional soft-knee observation (no hard clip): {soft}. This nonlinear knee is reported transparently, not presented as clean analog audio. Raw pre-master PCM and before-conversion hard clips must both pass. The matrix's master64 bound is the exact steady linear upper bound (raw/4); actual post-master peaks for every admitted patch are recorded in `runtime.log` at eight notes, with fade behavior retained.

Initial wall-clock maximum across the full host sweep: {observed_max}us. It was collected during firmware builds;58 otherwise-safe timing outliers were separately rechecked without a concurrent firmware build, maximum{retest_max}us. Original timings are retained. Three additional timing flags on already-rejected clipping/release patches are not waived. These are desktop scheduling observations, never P4 deadlines or latency. LOW/MEDIUM/HIGH host cost uses eight-note average thresholds<=50/<=120/>120us; the per-patch measured averages are retained in JSON. P4 cost class remains UNMEASURED until real board sampling against5333.33us plus a measured safety margin.

PASS: Harmony OFF PCM is byte-identical to0.9 for128/135/138/142/4/8/31 (128 stereo blocks each). This checks synthesis compatibility even where stricter qualification removes an old patch from selection. Existing retrigger, ownership, bend/CC1, sustain and Harmony suites are retained. Physical audio/latency/underruns and prolonged qualification: MANUAL / NOT RUN.
'''
    (docs/'factory_library_010_audio_analysis.md').write_text(audio,encoding='utf-8')
    memory='''# Factory library 0.10 — memory

PASS: `./smk-p4/tests/compare_factory_heap.ps1`, same AMY C objects/allocator/host ABI:

|Profile|Catalog selectable|Resident instruments|Oscillator capacity|Requested AMY heap B|Live tracked allocations|Runtime object B|
|---|---:|---:|---:|---:|---:|---:|
|0.9 baseline|15|8|64|214922|163|5540|
|0.10 candidate|229|8|64|214922|163|5540|

No heap growth from catalog expansion. These allocator-request bytes exclude host/C++ library overhead, static metadata, LVGL, stacks and drivers; they are not free internal ESP32 RAM readings. Full runtime stress visits each admitted patch three times,687 swaps, interleaving Juno/FM, sustained Harmony groups, eight notes and disconnect. Requested bytes remain stable per warmed layout; full teardown returns bytes and live allocations to baseline. Separate raw-engine stress traverses all256 IDs, including excluded sounds, with three complete passes (768 loads) and checked maps, plus the isolated512-load warm stress. Host fragmentation/largest free block cannot be inferred from requested-byte tracking; target measurements remain pending.

The first exploratory sweep started with48 Juno oscillators and found107594 requested bytes; visiting64 FM oscillators warmed extra DSP state to116618. Its first repeat correctly tripped the strict first-visit comparison. The final reproducible probe warms64 FM oscillators first and passes the three complete traversals. The exploratory failure/log is preserved, not counted as the successful final sweep. Runtime already boots in FM, so no production allocator workaround or pool change was needed.

All256 metadata rows and the admitted index/UI projection are generated constexpr rodata. No instrument is allocated by catalog definition or UI selection metadata. Names are preserved from ROM. The small generated admitted projection duplicates metadata pointers in flash, not AMY banks. Favorites grow from20 to256 RAM flags (+236B before ABI alignment); two small filter fields are added. Browser collection now stores only six visible pointers,24B on P4, rather than256 pointers; two bounded UI-only scans count and collect a page. No UI scan or display work enters the synthesis/output task.

Fixed128KiB LVGL pool, independent output queue, three preallocated PCM blocks,8 voices/64 oscillators, delta reserve, task stacks/priorities and boot/post-boot memory guards remain unchanged. No PSRAM allocator relocation, NVS, dependency upgrade or new driver. Target static sections/binary sizes are frozen in the candidate manifest. New-image free/largest internal RAM/PSRAM, stack margin, fragmentation, render load and DMA continuity are MANUAL / NOT RUN; old0.9 boot numbers are references only.
'''
    (docs/'factory_library_010_memory.md').write_text(memory,encoding='utf-8')
    category_lines='\n'.join(f'|{cat}|{count}|' for cat,count in sorted(categories.items()))
    (ROOT/'build/factory010/category_summary.md').write_text('|Category|Admitted|\n|---|---:|\n'+category_lines+'\n',encoding='utf-8')
    print('Generated audio/memory notes; categories',dict(categories))
if __name__=='__main__':main()
