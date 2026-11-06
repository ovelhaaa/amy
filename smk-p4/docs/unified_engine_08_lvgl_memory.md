# LVGL memory diagnosis0.8

Production allocator is unchanged: LVGL9.3 built-in TLSF,128KiB. Effective
sdkconfig has no fixed LV_MEM_ADR and no custom PSRAM pool allocator. The actual
link map places `.bss.work_mem_int.0` at0x4ff40000, size0x20000 in `.dram1.bss`:
this pool is static internal DRAM. Unused TLSF capacity does not become general
heap. Framebuffers are separate PSRAM allocations; DMA eligibility is unchanged.

Executed128KiB host experiment: `./smk-p4/ui-preview/run_preview.ps1` PASS.
All14 screens/subtabs and700 mounts passed with baseline/after free64504/64504
bytes, peak99152 bytes, fragmentation1%. The test workload is broader than the
historical physical0.7 observation near50KiB; do not substitute one for the other.

Executed isolated96KiB host experiment:
`./smk-p4/ui-preview/run_preview.ps1 -PoolKB 96` FAIL (expected diagnostic result).
LVGL style-array reallocation failed, asserted at `lv_obj_style.c:127` in
`LV_ASSERT_MALLOC(obj->styles)`. Before hardening the host assertion handler,
this failure spun indefinitely and the verified96KiB test process was stopped.
The repeat terminated with an explicit assertion. Host-only assertions now abort
instead of hanging. No target allocator, pool size or memory capability changed.

96KiB is insufficient for the executed full regression workload. Keep128KiB.
A narrower live-only96KiB test could be a separate future experiment; it would
not prove the complete demo/live lifecycle safe. Theoretical reservation change
is32KiB, not78KiB. Recoverable target heap requires a separately linked/measured
image and capability-aware before/after readings; none were obtained here.

PSRAM pool relocation is NOT RUN and not implemented. It requires explicit pool
ownership/lifecycle, suitable allocation capabilities, cache and refresh-cost
measurements, fragmentation, failure paths and all14 screens on target. No
claim of CPU/render improvement or recoverable heap follows from host results.

The reusable AMY architecture addresses duplicated residency independently of
LVGL. Pool optimization is not required to expose the15 software candidates.
