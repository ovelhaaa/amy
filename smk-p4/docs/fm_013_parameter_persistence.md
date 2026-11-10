# SMK-P4 0.13: FM Algorithm Parameter Persistence Specification

## 1. Overview
The `fm.algo` parameter is mapped to `smk::p4::edit::FmAlgorithm = 57` (`Count = 58`) within `edit::Mask` (`uint64_t`).
This document specifies how `FmAlgorithm` integrates with V1, V2, Compact Codec (Modes 0, 1, 2), NVS dual-bank transactional persistence, and backward compatibility.

## 2. Param ID Allocation & Backward Compatibility
- Parameter Enum:
```cpp
enum Param : uint8_t {
    Cutoff = 0, Resonance = 1, Attack = 2, Release = 3,
    OpLevel0 = 4, OpLevel5 = 9,
    OpRatio0 = 10, OpRatio5 = 15,
    Depth = 16,
    Decay = 17, Sustain = 18, FilterEnv = 19, FilterKeytrack = 20,
    OscLevel = 21, SubLevel = 22, NoiseLevel = 23, PulseWidth = 24, Detune = 25,
    PatchLevel = 26, Pan = 27, Velocity = 28,
    FmFeedback = 29,
    OpAttack0 = 30, OpAttack5 = 35,
    OpDecay0 = 36, OpDecay5 = 41,
    OpVel0 = 42, OpVel5 = 47,
    LfoRate = 48, LfoDepth = 49, LfoWave = 50, LfoDest = 51,
    ModEnable = 52, ModSource = 53, ModDest = 54, ModAmount = 55, ModPolarity = 56,
    FmAlgorithm = 57,
    Count = 58
};
```
- Frozen IDs: IDs 0..56 retain their exact bit position, meaning all V1 (0..16) and legacy V2 (0..56) records remain 100% valid and decodable.
- Mask Size: `sizeof(edit::Mask) == 8` (64 bits). `Count = 58 <= 64` static assert passes.

## 3. Compatibility Rule for Legacy Presets
- Legacy presets that do not set bit 57 (`FmAlgorithm`) in `p.mask` do **not** default to Algorithm 1.
- Instead, they fall back to the native factory algorithm captured from the preset's base patch (`out.original[edit::FmAlgorithm]`).
- For Juno patches (`family == 1`), `FmAlgorithm` is not in `allowedMask()` and is strictly rejected if set in mask.

## 4. V2 Sparse Codec Modes
A V2 record has a fixed size of 160 bytes. The 58 parameters are packed according to active count:
- **Mode 0 (active <= 20 fields)**:
  Packed as tag-value pairs: `b[pos++] = uint8_t(param_id); put32(b + pos, bits); pos += 4;` (5 bytes per param).
  With `active <= 20`, `pos = 52 + 20 * 5 = 152 <= 152`. Fits within budget.
- **Mode 1 (21 <= active <= 25 fields)**:
  Sequential raw 32-bit floats for all bits set in `p.mask`: `put32(b + pos, bits); pos += 4;` (4 bytes per param).
  `pos = 52 + 25 * 4 = 152 <= 152`. Fits within budget.
- **Mode 2 (active > 25 fields)**:
  Variable-width quantization:
  - Float params (4 bytes): `Depth`, `OpRatio0..5`, `LfoRate`.
  - Byte params (1 byte): `LfoWave`, `LfoDest`, `ModEnable`, `ModSource`, `ModDest`, `ModPolarity`, and **`FmAlgorithm`**.
    Because `FmAlgorithm` is integer 1..32, it serializes as a single uint8_t: `b[pos++] = uint8_t(lround(val))`.
  - Time params (2 bytes): `OpAttack0..5`, `OpDecay0..5` (16x scaled uint16).
  - Scaled 100x params (2 bytes): All remaining continuous percent parameters.
  Max capacity test: Even when all FM parameters are active simultaneously (OpLevel0..5, OpRatio0..5, OpAttack0..5, OpDecay0..5, OpVel0..5, Feedback, Brightness, Algorithm, LFO, Mod), Mode 2 occupies only ~88 bytes out of 100 available payload bytes.

## 5. Transactional Integrity
Preset Save, Overwrite, Recall, and Power-cycle execute through `PresetTransactionCoordinator`:
1. Dual-bank ping-pong in NVS namespace `user_presets`.
2. 32-bit sequence counter monotonicity.
3. Commit marker validation and CRC32 verification over bytes 0..155.
4. ROM compatibility key `0x3e76efb6`.
5. Atomic coordinated snapshot with FX persistence.
