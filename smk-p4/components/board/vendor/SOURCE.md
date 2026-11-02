# Provenance and scope

Hardware reference supplied by the user: `C:/progs/P4SDM`, commit
`307a8190152b12bba3e3a65fa81e17647d7c1cfd`, same physical board. That checkout
was read only; its pre-existing untracked `docs/m21/error.png` was preserved.

`board_p4_st7701_init.h` and `LICENSE-guition.txt` are copied unchanged from
that project's `src/hal/vendor/`. Original source:
[ultramcu BSP commit 324970bade0d1f4e52880fe8016580368bc1e06e](https://github.com/ultramcu/guition-jc4880p4-bsp/tree/324970bade0d1f4e52880fe8016580368bc1e06e).
MIT copyright and license are retained. No unlicensed third-party application
or register-table snippet has been imported from other boards.

`display_hal.*`, `display_dirty.h`, `display_geometry.h`, `touch_hal.*` and the
geometry/history tests adapt the user's P4SDM code. Changes: ESP-IDF component
registration, standalone board I2C owner (no Arduino), renamed feature guard,
and checked partial RGB565 bitmap import for LVGL. The native dirty-tile repair,
two-refresh retirement fence, ST7701S initialization and GT911 coordinate
transform remain the reference implementation. No P4SDM synth, sequencer,
sampler, audio HAL, persistence or UI was copied.

Physical profile: reset5, backlight23, touch reset3, I2C SDA7/SCL8, LDO3/2500mV,
DSI2×500Mbps, DPI34MHz, 480×800 native. Logical `(x,y)` maps to native
`(479-y,x)`; touch is transformed once, before LVGL. LVGL rotation stays zero.
This is not an electrical-equivalence claim for every JC4880P433-labelled board.

The native framebuffer submission is coupled to the pinned ESP-IDF DPI driver.
It explicitly flushes modified/repaired tile cache spans, then selects the
native framebuffer with a one-row `draw_bitmap` call. Keep the two-refresh
reuse fence and re-audit this path on any SDK change. Only the UI task waits.

LVGL is the only new downloaded runtime dependency: `lvgl/lvgl==9.3.0`, MIT,
from the Espressif Component Registry. Required for the P4 UI only, actively
maintained upstream; version deliberately fixed for this port. Draw buffer:
51,200 PSRAM bytes; native framebuffers: 2,304,000 PSRAM bytes; configured LVGL
pool: 128 KiB. Widget allocations occur in the P4 UI owner, never in the S3
audio path. Exact linked Flash/static RAM cost must come from the build report;
neither the base LVGL size nor these payload sizes represent measured peak heap.
