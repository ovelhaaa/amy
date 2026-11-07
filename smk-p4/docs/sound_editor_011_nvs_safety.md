# NVS safety0.11

Partition unchanged: NVS0x9000..0xefff (24KiB), factory app at0x10000.
No SD or dependency upgrade. ESP-IDF5.5.2's bundled NVS is used (existing SDK).
Initialization fails without erasing existing data on NO_FREE_PAGES/version/error.
The namespace admission checks nvs_get_stats and nvs_get_used_entry_count:
free entries plus our existing entries must cover16*2*7 blob entries plus126 GC
margin. This conservative bound is not a measurement of currently occupied NVS;
physical free/used entries must be recorded at hardware qualification.

16*2*128=4096 payload bytes maximum, plus NVS headers/index/GC.32 slots were not
selected because physical available capacity and fragmentation were not measured.
Storage errors disable persistence while retaining factory audio. Never erase or
resize NVS automatically. Explicit user Save As is the only preset-write trigger;
slider and macro moves do not write Flash.

NVS init/open runs before audio/USB startup. Save runs on the control/UI task,
not synthesis/DMA/ISR. The owner acknowledges a maintenance quiet barrier first.
Pending gate state is cleared through existing Panic/ownership logic; USB/Touch
input during maintenance is invalidated. The PCM owner continues receiving zeros.
A missing quiet ACK times out after2s without writing. Cache-disabled periods
inside ESP-IDF Flash operations may still delay output scheduling; physical DMA
continuity during this silent interval is MANUAL. Do not promise gapless saving.
Play/hold is rejected while busy, and save admission requires no confirmed gates.
Release tails are intentionally cut by this explicitly requested maintenance.

Two independent blob keys provide recovery if the destination copy is incomplete.
No previous valid key is explicitly erased. Per-key commit/readback errors are
checked. NVS internal GC/power-failure guarantees are not replaced by the host
fake store: power cuts and restart on the actual board remain MANUAL.

Before flashing: preserve the current full Flash including NVS, compare partition
bytes, reconfirm device/port, authorize the exact candidate hash. App-only flashing
must retain NVS/bootloader/partitions. Rollback may require restoring0.10 app and,
only if needed and explicitly authorized, the independently preserved full image.
