# Factory library 0.10 — memory

PASS: `./smk-p4/tests/compare_factory_heap.ps1`, same AMY C objects/allocator/host ABI:

|Profile|Catalog selectable|Resident instruments|Oscillator capacity|Requested AMY heap B|Live tracked allocations|Runtime object B|
|---|---:|---:|---:|---:|---:|---:|
|0.9 baseline|15|8|64|214922|163|5540|
|0.10 candidate|229|8|64|214922|163|5540|

No heap growth from catalog expansion. These allocator-request bytes exclude host/C++ library overhead, static metadata, LVGL, stacks and drivers; they are not free internal ESP32 RAM readings. Full runtime stress visits each admitted patch three times,687 swaps, interleaving Juno/FM, sustained Harmony groups, eight notes and disconnect. Requested bytes remain stable per warmed layout; full teardown returns bytes and live allocations to baseline. Separate raw-engine stress traverses all256 IDs, including excluded sounds, with three complete passes (768 loads) and checked maps, plus the isolated512-load warm stress. Host fragmentation/largest free block cannot be inferred from requested-byte tracking; target measurements remain pending.

The first exploratory sweep started with48 Juno oscillators and found107594 requested bytes; visiting64 FM oscillators warmed extra DSP state to116618. Its first repeat correctly tripped the strict first-visit comparison. The final reproducible probe warms64 FM oscillators first and passes the three complete traversals. The exploratory failure/log is preserved, not counted as the successful final sweep. Runtime already boots in FM, so no production allocator workaround or pool change was needed.

All256 metadata rows and the admitted index/UI projection are generated constexpr rodata. No instrument is allocated by catalog definition or UI selection metadata. Names are preserved from ROM. The small generated admitted projection duplicates metadata pointers in flash, not AMY banks. Favorites grow from20 to256 RAM flags (+236B before ABI alignment); two small filter fields are added. Browser collection now stores only six visible pointers,24B on P4, rather than256 pointers; two bounded UI-only scans count and collect a page. No UI scan or display work enters the synthesis/output task.

Fixed128KiB LVGL pool, independent output queue, three preallocated PCM blocks,8 voices/64 oscillators, delta reserve, task stacks/priorities and boot/post-boot memory guards remain unchanged. No PSRAM allocator relocation, NVS, dependency upgrade or new driver. Target static sections/binary sizes are frozen in the candidate manifest. New-image free/largest internal RAM/PSRAM, stack margin, fragmentation, render load and DMA continuity are MANUAL / NOT RUN; old0.9 boot numbers are references only.
