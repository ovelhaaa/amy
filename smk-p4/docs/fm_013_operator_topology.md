# SMK-P4 0.13: Operator Classification and FM Topology Routing

## 1. Scope
This document details the dynamic classification of operators and topology derivation across the 32 DX7 algorithms in AMY.

## 2. Bus Semantics in AMY Core
AMY's FM rendering engine (`src/algorithms.c`) uses a scratch block partitioned into:
- `BUS_ONE`: Primary modulation bus
- `BUS_TWO`: Secondary modulation bus
- `SCRATCH`: Scratch buffer for swap operations (used when reading `IN_BUS_ONE` and writing `OUT_BUS_ONE`)
- `buf`: The voice audio output buffer

Operators execute sequentially from `native_op = 0` (DX7 OP6) to `native_op = 5` (DX7 OP1).
Each operator is configured with bitwise flags:
- `OUT_BUS_ONE` (0x01): Writes to `BUS_ONE`.
- `OUT_BUS_TWO` (0x02): Writes to `BUS_TWO`.
- `OUT_BUS_ADD` (0x04): Accumulates into target buffer without clearing it first.
- `IN_BUS_ONE`  (0x10): Reads modulation input from `BUS_ONE`.
- `IN_BUS_TWO`  (0x20): Reads modulation input from `BUS_TWO`.
- `FB_IN`       (0x40): Receives feedback level from `synth[osc]->feedback`.
- `FB_OUT`      (0x80): Writes output sample into the voice feedback history delay line.

## 3. Dynamic Classification Rule
In SMK-P4 runtime:
```cpp
bool Runtime::updateFmAlgorithmTopology(unsigned algorithm) {
    if(algorithm < 1 || algorithm > 32 || state_.edit.family != 0) return false;
    for(unsigned op = 0; op < 6; ++op) {
        const unsigned native_op = 5 - op;
        const uint8_t flags = algorithms[algorithm].ops[native_op];
        // If the operator directs its output to BUS_ONE or BUS_TWO, it is a modulator.
        // Otherwise, it accumulates directly into audio buf (Carrier).
        edit_modulators_[op] = ((flags & 3) != 0);
    }
    return true;
}
```

### Carrier vs Modulator Definition
- **Carrier (`edit_modulators_[op] == false`)**:
  Output goes to audio `buf`.
  Gain is nominal operator output level: `b.amp * (value(edit::OpLevel0+op) / 100.0f) * 1.0f`.
- **Modulator (`edit_modulators_[op] == true`)**:
  Output goes to `BUS_ONE` or `BUS_TWO`.
  Gain is modulated by `fm.depth` (`FM BRIGHTNESS`): `b.amp * (value(edit::OpLevel0+op) / 100.0f) * value(edit::Depth)`.

## 4. FM Brightness Protection
When switching between algorithms, an operator that shifts from modulator to carrier (e.g., OP2 in Algo 1 vs Algo 32) immediately has its amplitude re-emitted with factor `1.0f`, completely removing the modulator attenuation.
Conversely, an operator shifting from carrier to modulator immediately receives the `fm.depth` multiplier.
This guarantees no stale gain remains on any operator when topology changes.

## 5. Verification Matrix of the 32 Algorithms

| Algo | Carrier Count | Modulator Count | Carriers (UI 1-based) | Feedback Op |
|:----:|:-------------:|:---------------:|:---------------------:|:-----------:|
| 1 | 2 | 4 | OP1, OP3 | OP6 |
| 2 | 2 | 4 | OP1, OP3 | OP2 |
| 3 | 2 | 4 | OP1, OP4 | OP6 |
| 4 | 2 | 4 | OP1, OP4 | OP4 |
| 5 | 3 | 3 | OP1, OP3, OP5 | OP6 |
| 6 | 3 | 3 | OP1, OP3, OP5 | OP5 |
| 7 | 2 | 4 | OP1, OP3 | OP6 |
| 8 | 2 | 4 | OP1, OP3 | OP4 |
| 9 | 2 | 4 | OP1, OP3 | OP2 |
| 10 | 2 | 4 | OP1, OP4 | OP3 |
| 11 | 2 | 4 | OP1, OP4 | OP6 |
| 12 | 2 | 4 | OP1, OP4 | OP2 |
| 13 | 2 | 4 | OP1, OP4 | OP6 |
| 14 | 2 | 4 | OP1, OP3 | OP6 |
| 15 | 2 | 4 | OP1, OP3 | OP2 |
| 16 | 3 | 3 | OP1, OP3, OP4 | OP6 |
| 17 | 3 | 3 | OP1, OP3, OP4 | OP4 |
| 18 | 3 | 3 | OP1, OP2, OP4 | OP4 |
| 19 | 2 | 4 | OP1, OP4 | OP6 |
| 20 | 2 | 4 | OP1, OP4 | OP3 |
| 21 | 3 | 3 | OP1, OP4, OP5 | OP3 |
| 22 | 4 | 2 | OP1, OP4, OP5, OP6 | OP6 |
| 23 | 4 | 2 | OP1, OP4, OP5, OP6 | OP6 |
| 24 | 5 | 1 | OP1, OP3, OP4, OP5, OP6 | OP6 |
| 25 | 5 | 1 | OP1, OP3, OP4, OP5, OP6 | OP6 |
| 26 | 5 | 1 | OP1, OP3, OP4, OP5, OP6 | OP6 |
| 27 | 5 | 1 | OP1, OP3, OP4, OP5, OP6 | OP3 |
| 28 | 3 | 3 | OP1, OP3, OP5 | OP5 |
| 29 | 4 | 2 | OP1, OP3, OP4, OP5 | OP6 |
| 30 | 4 | 2 | OP1, OP3, OP4, OP5 | OP5 |
| 31 | 5 | 1 | OP1, OP2, OP3, OP4, OP5 | OP6 |
| 32 | 6 | 0 | OP1, OP2, OP3, OP4, OP5, OP6 | OP6 |

Every algorithm satisfies `carriers + modulators == 6`.
Derivation matches AMY's DSP loop identically.

---

## 6. Graphical Connection Engine & LVGL Visualization (Gate C)

In addition to textual routing formulas, release 0.13 introduces real graphical line routing in LVGL 9.3:
- **Topology Engine (`fm_topology.h`)**:
  - `generateLayout(topo, width, height)` computes fixed 2D coordinates for all 6 operator boxes and output bus.
  - Generates discrete connection edges:
    - **Modulation Routes (`EdgeType::Modulation`)**: Displayed in bright Amber (`#F59E0B`), connecting modulator operator output to carrier/modulator phase input.
    - **Audio Routes (`EdgeType::AudioOut`)**: Displayed in bright Cyan (`#06B6D4`), routing carriers directly to the audio output terminal (`OUT`).
    - **Feedback Loops (`EdgeType::Feedback`)**: Displayed in vivid Orange (`#FF5722`), drawing a 4-point loop indicating operator self-feedback.
- **Real-Time Display Safety**:
  - Exactly 20 reusable `lv_line` objects and point arrays are preallocated in `TopologyDiagramCtx`.
  - Coordinates are updated in place upon topology refresh with zero dynamic heap allocations per frame (`malloc` count = 0).
  - Operator nodes display real-time selection border, carrier vs. modulator color coding, and feedback badge.
