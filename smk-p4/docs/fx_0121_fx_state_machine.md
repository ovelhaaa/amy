# FX State Machine & Life Cycle
## SMK-P4 0.12.1

### Overview
The FX subsystem connects the UI and control tasks with the real-time audio thread executing AMY. The design guarantees zero dynamic memory allocations in the audio thread, lock-free wait-free communication, and atomic lifecycle transitions.

```
+------------------+         +-------------------------------+         +-------------------+
|  UI / Control    |  push   |       Commands::fx            |   pop   |    Audio Thread   |
|  (Task Priority) | ------> |  FxQueue<8> (Lock-Free Ring)  | ------> |    (applyFx)      |
+------------------+         +-------------------------------+         +-------------------+
        ^                                                                        |
        |                            publish                                     |
        +------------------------------------------------------------------------+
```

### State Variables
In `LocalAmyPerformanceBackend`:
- `desired_fx_`: Live FX configuration currently edited or requested.
- `original_fx_`: Confirmed snapshot configuration corresponding to the currently loaded patch.
- `saved_fx_`: Configuration persisted to NVS in the active preset slot.
- `fx_pending_`: Flag indicating a command is waiting in the dispatch queue.
- `fx_token_`: Monotonically increasing request identifier.

### Dirty State Tracking (`unsaved()`)
A preset is considered `unsaved()` if either synthesis parameters or FX parameters differ from the last confirmed/saved baseline:
- `synthUnsaved()`: Evaluates manual sound edits vs `saved_edit_`.
- `fxUnsaved()`: Evaluates `desired_fx_` vs `saved_fx_` using `fxEquivalent()`.
- Notice display:
  - If synth is edited: `"EDITED / UNSAVED"`.
  - If only FX is edited: `"FX EDITED / UNSAVED"`.
  - If both are unchanged: `"FACTORY ORIGINAL"` or preset name.

### Accidental Loss Prevention
When `unsaved()` is true, patch switching is blocked:
```cpp
if (!recalling_ && unsaved()) return false;
```
The operator must either Save As (`editAction(2, ...)`), Revert (`editAction(1)`), or Compare (`editAction(0)`) before switching to another patch.

### Compare / Revert Semantics
- **Compare (Action 0)**: Toggles live sound between edited state and original state without losing edits.
- **Revert (Action 1)**: Restores `desired_fx_` to `original_fx_`, pushes restored configuration to the audio engine, and clears unsaved state.

### Stale Command Invalidation
When a patch is changed or a Panic is triggered:
1. `commands_.panic()` calls `fx.clear()`, wiping any unprocessed requests in the queue.
2. `commands_.generation()` increments.
3. In `applyFx()`, requests with `generation != 0` that do not match the current generation are dropped, incrementing `state_.fx.rejected`.
