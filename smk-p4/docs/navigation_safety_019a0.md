# 0.19-A.0 navigation safety

The navigation manager rejects direct, Back, area, HOME and same-screen rebuild requests while a screen confirmation or the externally mounted User Presets modal is open. Deferred execution checks the same policy after consuming its request. Blocked requests do not change history or sub-tabs. Footer areas check before changing overlays. PANIC cancels the User Presets panel and retains the existing scene cancellation behavior; it never invokes a destructive confirm callback.

PASS: focused GCC C++20/O2/Wall/Wextra/Werror link using the 0.18-D.1 host build flags and AMY/LVGL objects, executing the expanded ui_018d1_test.cpp. Covers all five areas, direct navigation, Back, queued-before-modal execution, MIDI HOME/NextPage/Learn packets, Panic/cancel, Save Pattern, User Presets and 1000 navigation stress iterations. LVGL objects 93 to 93; free 74192 to 74192; largest 73184; peak 68736 bytes. No Audio Owner changes. Initial test fixture incorrectly issued Panic before assertions requiring active transport; fixture sequencing was corrected. Full provenance rebuild is separate and still required.

MANUAL: physical touch/USB qualification deferred by user. No pin or driver changes.
