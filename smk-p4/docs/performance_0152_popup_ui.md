# SMK-P4 0.15.2 — Patch Load Popup & Performance Lock UX

## 1. Overview

In SMK-P4 0.15.2, the patch load confirmation dialog was redesigned to support **Performance Lock** on the 800×480 touchscreen (Guition JC4880P433).

The popup allows the performer to either perform a clean patch change or selectively preserve active performance contexts (Audio FX, Harmony / Chord Assist, Arpeggiator, and Performance Macros).

---

## 2. Layout & Visual Architecture

```text
┌────────────────────────────────────────────────────────────────────────┐
│ LOAD PATCH 128 — DX7 BRASS 1?                                          │
│ Selective Performance Lock: choose settings to preserve.               │
│                                                                        │
│ [x] KEEP FX (CHORUS/DLY/REV/EQ)      [x] KEEP HARMONY / CHORD          │
│                                                                        │
│ [x] KEEP ARPEGGIATOR (RATES/CFG)     [ ] KEEP MACROS (ADV)             │
│                                                                        │
│                                                                        │
│ [  CANCEL  ]    [  LOAD PATCH  ]       [  LOAD WITH LOCKS  ]           │
└────────────────────────────────────────────────────────────────────────┘
```

- **Panel Dimensions:** 620 px wide × 276 px high (centered, maximum height <= 392 px).
- **Background Dimming:** 70% black overlay (`LV_OPA_70`).
- **Heading (y = 12, Montserrat 20):** `LOAD PATCH %03u — %s?` (with resolved patch name).
- **Subtitle (y = 42, Montserrat 14):**
  - Clean state: `Selective Performance Lock: choose settings to preserve.` (accent gold).
  - Dirty state: `Unsaved changes to the current patch will be lost.` (warning orange).
- **Checkboxes (2×2 Grid, 280×36 px touch hitboxes):**
  - Row 1: `KEEP FX (CHORUS/DLY/REV/EQ)` (x = 16) | `KEEP HARMONY / CHORD` (x = 310)
  - Row 2: `KEEP ARPEGGIATOR (RATES/CFG)` (x = 16) | `KEEP MACROS (ADV)` (x = 310)
- **Status Label (y = 168):** Displays validation or discard warnings.
- **Action Buttons (y = 206, h = 44 px):**
  - Clean State:
    - `[ CANCEL ]` (w = 110 px): dismisses popup without action.
    - `[ LOAD PATCH ]` (w = 160 px): loads target patch clean (`mask = 0`).
    - `[ LOAD WITH LOCKS ]` (w = 298 px, accent): loads target patch preserving checked contexts.
  - Dirty State (unsaved sound edits):
    - `[ CANCEL ]` (w = 110 px): dismisses popup without action.
    - `[ DISCARD & LOAD ]` (w = 170 px): discards edits and loads clean (`mask = 0`).
    - `[ DISCARD & LOAD WITH LOCKS ]` (w = 288 px, accent): discards synth edits but preserves checked performance contexts.

---

## 3. Session State & Selection Policies

### 3.1 Session Retention
- Active checkbox states are retained in static module memory `s_session_lock_mask`.
- Boot default: `perf_lock::kDefaultLockMask = Fx | Harmony | Arp` (0x07).
- Modifying any checkbox in the popup updates `s_session_lock_mask` immediately.

### 3.2 Fast vs Assisted Selection Policy
1. **Patch Browser (Assisted Selection):**
   - Tapping any patch row opens the confirmation popup (`force_dialog = true`).
   - Performer can review the patch name, inspect/toggle locks, and confirm.
2. **NEXT / PREV Buttons & SMK25 Pad Navigation (Fast Selection):**
   - Clean state: switches patches immediately using the active session mask (`s_session_lock_mask`) without popup interruption.
   - Dirty state: opens the confirmation popup to prevent accidental loss of unsaved sound edits.
