# SMK-P4 0.13: Native FM Algorithm Selector — AMY Architecture Audit

## 1. Overview and Objectives
In SMK-P4, FM synthesis is powered by AMY's 6-operator DX7 implementation (`wave == ALGO`).
Prior to version 0.13, operator parameters (levels, ratios, attack, decay, velocity sensitivity, feedback, brightness) were editable, but `fm.algo` was marked as `Planned` (read-only placeholder).

This audit investigates AMY's native FM algorithm routing implementation across:
- `src/algorithms.c`
- `src/amy.h`
- `src/amy.c`
- `src/patches.c`
- `src/oscillators.c`
- `smk-p4/components/amy_runtime/sound_edit.h`
- `smk-p4/components/amy_runtime/sound_edit_runtime.inc`
- `smk-p4/components/amy_runtime/amy_runtime.cpp`
- `smk-p4/components/amy_runtime/amy_runtime.h`
- `smk-p4/components/amy_runtime/user_presets_v2.h`
- `smk-p4/components/ui_blueprint/screens/sound_editor_screen.cpp`
- `smk-p4/components/ui_blueprint/src/ui_parameters.cpp`
- `smk-p4/main/ui_blueprint_performance_adapter.cpp`
- `smk-p4/main/local_performance_backend.h`

---

## 2. Answers to Mandatory Audit Questions

### Q1: Can the `ALGORITHM` parameter be modified without recreating operators?
**Yes.**
In `src/amy.c:1643-1649`:
```c
if(d->param == ALGORITHM) {
    synth[d->osc]->algorithm = d->data.i;
    synth[d->osc]->eg_type[0] = ENVELOPE_DX7;
    synth[d->osc]->eg_type[1] = ENVELOPE_TRUE_EXPONENTIAL;
}
```
Setting `ALGORITHM` modifies `synth[osc]->algorithm` directly in the parent ALGO oscillator. It does not destroy, deallocate, or recreate the 6 operator oscillators in `synth[osc]->algo_source[0..5]`. In `src/algorithms.c:204`, `render_algo()` reads `algorithms[synth[osc]->algorithm]` dynamically at the beginning of each 256-sample audio render block.

### Q2: Is it necessary to modify `algo_source` when changing only topology?
**No.**
`algo_source[0..5]` holds the physical oscillator IDs assigned to the 6 DX7 operators (ordered native OP6 down to OP1). Changing the FM algorithm only changes the routing among these 6 materialized operators (bus writes, bus reads, feedback, audio buffer accumulation). The physical operator oscillators remain identical and retain their respective roles (`SYNTH_IS_ALGO_SOURCE`).

### Q3: How do the algorithms identify carriers and modulators?
In `src/algorithms.c`:
```c
enum FmOperatorFlags {
    OUT_BUS_ONE = 1 << 0, // 0x01
    OUT_BUS_TWO = 1 << 1, // 0x02
    OUT_BUS_ADD = 1 << 2, // 0x04
    IN_BUS_ONE  = 1 << 4, // 0x10
    IN_BUS_TWO  = 1 << 5, // 0x20
    FB_IN       = 1 << 6, // 0x40
    FB_OUT      = 1 << 7  // 0x80
};
```
In `render_algo()`:
- If `(algo.ops[op] & (OUT_BUS_ONE | OUT_BUS_TWO)) != 0` (i.e. `& 3 != 0`), the operator's output buffer is directed to `BUS_ONE` or `BUS_TWO`. It acts as a **Modulator** modulating subsequent operators.
- If `(algo.ops[op] & (OUT_BUS_ONE | OUT_BUS_TWO)) == 0`, the operator's output buffer is directed to `out_buf = buf` (the voice audio output buffer). It acts as a **Carrier** contributing directly to the audible sound.

### Q4: How is feedback routed?
In `src/algorithms.c`:
- The master `ALGO` oscillator stores `feedback = synth[osc]->feedback`.
- For each operator in `algo.ops[op]`, if `algo.ops[op] & FB_IN`, `render_algo()` supplies `feedback_level = F2S(synth[osc]->feedback)` to `render_mod(...)`.
- If `algo.ops[op] & FB_OUT`, the operator's output sample history is captured into the feedback delay line for sine FM (`render_fm_sine`).
- In all 32 DX7 algorithms, exactly one operator (or self/cross loop) is marked with `FB_IN` and `FB_OUT`.

### Q5: How does the editor identify OP1–OP6?
In `src/algorithms.c`, the native evaluation loop iterates `op = 0 .. 5`, corresponding to DX7 **OP6 down to OP1**:
- Native `op = 0` -> DX7 OP6
- Native `op = 1` -> DX7 OP5
- Native `op = 2` -> DX7 OP4
- Native `op = 3` -> DX7 OP3
- Native `op = 4` -> DX7 OP2
- Native `op = 5` -> DX7 OP1

In SMK-P4, the UI and user-facing editor use `ui_op = 0 .. 5` representing OP1 to OP6.
Therefore:
`native_op = 5 - ui_op`
`edit_ops_[ui_op]` corresponds to `algo_source[5 - ui_op]`.

### Q6: Must the update occur across all eight voices?
**Yes.**
SMK-P4 operates with 8 polyphonic voices (`kVoices = 8`). Each voice has its own parent ALGO oscillator at `edit_map_[v][0]`. When an algorithm change is requested, all 8 parent oscillators must receive the updated `ALGORITHM` delta atomically within the same engine boundary, ensuring polyphonic consistency across voices.

### Q7: What states must be updated after an algorithm change?
1. `synth[edit_map_[v][0]]->algorithm` across all 8 voices.
2. `edit_modulators_[6]` boolean array (derived from `(algorithms[new_algo].ops[5 - op] & 3) != 0`).
3. Recomputation and re-emission of effective operator gain levels (`e.amp_coefs[COEF_CONST]`), because `fm.depth` (FM Brightness) scales modulators by `value(edit::Depth)` while carriers receive unscaled nominal gain (`1.0f`). Any operator transitioning from modulator to carrier or vice versa must have its amplitude updated immediately.
4. Editor snapshot state (`out.manual[FmAlgorithm]`, `out.applied[FmAlgorithm]`, `out.dirty`, `out.mask`).
5. UI model representation (`model_.setInt("fm.algo", new_algo)`).
6. Graphical topology display on the touchscreen.

### Q8: How to prevent stale commands during Patch Change and Recall?
1. **Token & Generation Guards:** `edit::Request` contains `generation` and `patch`. When a patch changes, `state_.generation` increments and `patch_id` changes; any queued or obsolete request is rejected immediately (`request.generation != state_.generation || request.patch != out.patch`).
2. **Quiescence Barrier:** Patch Change and Recall set `busy()` and wait for engine quiescence (`active_notes == 0`).
3. **Execution Safety Policy:** Algorithm selection is rejected if `active_notes > 0`, if MIDI sustain is active, or if save/recall is in flight, returning `"RELEASE NOTES TO CHANGE FM ALGORITHM"`.

---

## 3. Native Algorithm Routing Table Summary

| Algo | Carriers (UI 1-based) | Modulators (UI 1-based) | Feedback Op |
|:----:|:---------------------:|:-----------------------:|:-----------:|
| 1 | 1, 3 | 2 (mod 1); 4, 5, 6 (mod 3) | 6 |
| 2 | 1, 3 | 2 (mod 1); 4, 5, 6 (mod 3) | 2 |
| 3 | 1, 4 | 2, 3 (mod 1); 5, 6 (mod 4) | 6 |
| 4 | 1, 4 | 2, 3 (mod 1); 5, 6 (mod 4) | 4 |
| 5 | 1, 3, 5 | 2 (mod 1); 4 (mod 3); 6 (mod 5) | 6 |
| 6 | 1, 3, 5 | 2 (mod 1); 4 (mod 3); 6 (mod 5) | 5 |
| 7 | 1, 3 | 2 (mod 1); 4, 5, 6 (mod 3) | 6 |
| 8 | 1, 3 | 2 (mod 1); 4, 5, 6 (mod 3) | 4 |
| 9 | 1, 3 | 2 (mod 1); 4, 5, 6 (mod 3) | 2 |
| 10 | 1, 4 | 2, 3 (mod 1); 5, 6 (mod 4) | 3 |
| 11 | 1, 4 | 2, 3 (mod 1); 5, 6 (mod 4) | 6 |
| 12 | 1, 4 | 2, 3 (mod 1); 5, 6 (mod 4) | 2 |
| 13 | 1, 4 | 2, 3 (mod 1); 5, 6 (mod 4) | 6 |
| 14 | 1, 3 | 2 (mod 1); 4, 5, 6 (mod 3) | 6 |
| 15 | 1, 3 | 2 (mod 1); 4, 5, 6 (mod 3) | 2 |
| 16 | 1, 3, 4 | 2 (mod 1); 5, 6 (mod 3) | 6 |
| 17 | 1, 3, 4 | 2 (mod 1); 5, 6 (mod 3) | 4 |
| 18 | 1, 2, 4 | 3 (mod 2); 5, 6 (mod 4) | 4 |
| 19 | 1, 4 | 2, 3 (mod 1); 5, 6 (mod 4) | 6 |
| 20 | 1, 4 | 2, 3 (mod 1); 5, 6 (mod 4) | 3 |
| 21 | 1, 4, 5 | 2, 3 (mod 1); 6 (mod 5) | 3 |
| 22 | 1, 4, 5, 6 | 2, 3 (mod 1) | 6 |
| 23 | 1, 4, 5, 6 | 2, 3 (mod 1) | 6 |
| 24 | 1, 3, 4, 5, 6 | 2 (mod 1) | 6 |
| 25 | 1, 3, 4, 5, 6 | 2 (mod 1) | 6 |
| 26 | 1, 3, 4, 5, 6 | 2 (mod 1) | 6 |
| 27 | 1, 3, 4, 5, 6 | 2 (mod 1) | 3 |
| 28 | 1, 3, 5 | 2 (mod 1); 4 (mod 3); 6 (mod 5) | 5 |
| 29 | 1, 3, 4, 5 | 2 (mod 1); 6 (mod 5) | 6 |
| 30 | 1, 3, 4, 5 | 2 (mod 1); 6 (mod 5) | 5 |
| 31 | 1, 2, 3, 4, 5 | 6 (mod all carriers) | 6 |
| 32 | 1, 2, 3, 4, 5, 6 | None (Additive Sinusoids) | 6 |

---

## 4. Conclusion
AMY's native API is fully sufficient for runtime algorithm changes via the `ALGORITHM` parameter applied to the parent oscillator. No modifications to AMY core or `src/algorithms.c` are necessary or permitted.
Proceeding to Gate B.
