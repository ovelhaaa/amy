# AGENTS.md

## Purpose

This repository is being adapted from the upstream AMY project into an autonomous embedded synthesizer for:

- ESP32-S3 DevKitC N16R8
- M-VAVE SMK25 V2 over USB MIDI Host
- PCM5102A over I²S
- 284 × 76 widescreen display
- standalone operation without a computer

The complete product specification is in `specs.md`.

This file defines how coding agents must work in this repository.

---

## 1. Mandatory reading order

Before making any change, read:

1. `AGENTS.md`
2. `specs.md`
3. the repository `README`
4. build files and dependency declarations
5. the AMY platform integration relevant to ESP32/ESP32-S3
6. the code directly related to the requested change
7. existing tests, examples, and CI configuration

Do not start implementation from assumptions about the repository structure.

Inspect the current tree first.

---

## 2. Source of truth

The priority order for decisions is:

1. explicit user instructions
2. `specs.md`
3. this `AGENTS.md`
4. existing repository architecture and conventions
5. upstream AMY documentation and code
6. reasonable engineering judgment

If `specs.md` conflicts with the current repository implementation, do not silently choose one.

Document the conflict and implement the smallest safe step that preserves compatibility.

---

## 3. Core engineering priority

The highest priority is stable, playable audio.

Use this order of priorities:

1. no crashes
2. no stuck notes
3. no audio underruns
4. low and predictable latency
5. correct MIDI behavior
6. USB reconnect stability
7. deterministic patch behavior
8. readable user interface
9. additional features

A smaller stable implementation is better than a large fragile implementation.

---

## 4. Scope discipline

Work incrementally.

Do not attempt the full synthesizer in one change.

The expected implementation order is:

1. repository and build validation
2. PCM5102A I²S test output
3. AMY audio rendering
4. USB MIDI Host enumeration
5. Note On and Note Off
6. velocity
7. pitch bend
8. modulation
9. disconnect, reconnect, and Panic behavior
10. display driver
11. MIDI monitor and controller mapping
12. patch management and macros
13. layers, pads, arpeggiator, sequencer, and scenes

Do not implement later stages before earlier stages have measurable acceptance tests.

---

## 5. Preserve upstream AMY

Treat AMY as an upstream dependency.

Prefer one of these approaches:

1. add a separate application layer around AMY
2. add isolated platform adapters
3. add new components outside AMY core
4. use narrowly scoped patches when upstream modification is unavoidable

Do not perform broad refactors of the AMY core merely to match the application architecture.

Do not rename, move, or rewrite upstream files without a clear technical need.

When changing upstream-derived code:

- keep the diff minimal
- explain why extension points were insufficient
- avoid breaking other AMY platforms
- preserve existing APIs whenever possible
- add compile-time guards for platform-specific behavior
- identify whether the change could be proposed upstream

Maintain a clear boundary between:

- AMY engine
- ESP32-S3 platform integration
- SMK-S3 application code

---

## 6. Repository organization

Prefer an architecture equivalent to:

```text
application
├── audio
├── midi
├── synth
├── sequencer
├── ui
├── storage
└── system

upstream
└── amy
```

Exact paths must follow the actual repository structure.

Do not invent directories before inspecting existing conventions.

New application modules should avoid direct access to AMY internals.

Use an adapter such as:

```cpp
class SynthEngine {
public:
    virtual ~SynthEngine() = default;

    virtual bool begin(uint32_t sample_rate) = 0;
    virtual void note_on(uint8_t channel, uint8_t note, uint8_t velocity) = 0;
    virtual void note_off(uint8_t channel, uint8_t note) = 0;
    virtual void pitch_bend(uint8_t channel, int16_t value) = 0;
    virtual void control_change(
        uint8_t channel,
        uint8_t controller,
        uint8_t value
    ) = 0;
};
```

The interface may be adapted to repository conventions, but the dependency direction must remain:

```text
UI / MIDI / Sequencer
        ↓
application events and parameters
        ↓
AMY adapter
        ↓
AMY
```

---

## 7. Real-time audio rules

The audio path is hard real-time from the application perspective.

Inside the audio render path, I²S callback, DMA refill path, or highest-priority audio task, do not:

- allocate or free heap memory
- parse JSON
- access files
- write to NVS or Flash
- render display content
- issue normal logs
- wait on long mutexes
- call blocking APIs
- enumerate USB devices
- perform controller mapping
- resize containers
- use unbounded loops
- perform large memory copies without measurement

Preallocate:

- audio buffers
- voice state
- event queues
- conversion buffers
- temporary DSP storage

Use bounded queues and deterministic failure behavior.

Preserve high-priority events when queues are under pressure:

1. Panic
2. All Notes Off
3. Note Off
4. Stop
5. Note On
6. pitch and modulation
7. low-priority continuous controls
8. UI-only events

A queue overflow must increment a diagnostic counter and must not block the audio task.

---

## 8. PCM5102A requirements

The initial audio target is PCM5102A over I²S.

Do not invent final GPIO assignments.

Pin assignments must remain configurable until the exact board and wiring are confirmed.

Required signals:

- BCLK
- LRCLK or WS
- DATA
- power
- ground

Initial target configuration:

- 48 kHz
- stereo
- I²S output
- DMA-backed transfer
- zeroed buffers before start
- controlled fade-in
- no audible boot transient when avoidable

Before AMY integration, provide a test mode capable of generating:

- silence
- left-only tone
- right-only tone
- stereo tone
- configurable test frequency
- clipping-safe amplitude

Measure and report:

- underrun count
- frames rendered
- maximum render duration
- sample rate configuration
- DMA buffer configuration

Do not claim clean audio or low latency without a test procedure.

---

## 9. USB MIDI Host rules

The ESP32-S3 native USB interface is intended to act as USB MIDI Host for the SMK25 V2.

Do not assume the exact MIDI messages sent by its controls.

Implement or preserve support for inspecting:

- USB descriptors
- endpoint configuration
- MIDI packet stream
- channel
- status byte
- data bytes
- timestamps
- disconnect and reconnect events

Required initial MIDI behavior:

- Note On
- Note Off
- Note On with velocity zero as Note Off
- velocity
- pitch bend
- modulation or generic CC
- All Notes Off
- sustain, if received
- disconnect-triggered Panic

The controller mapping must be data-driven.

Do not hard-code final CC numbers before observing the actual device or loading a confirmed profile.

---

## 10. Internal event model

Convert external MIDI input into an application event model.

Do not pass raw USB packets throughout the application.

Events should be bounded, timestamped, and independent of transport.

A suitable model may include:

```cpp
enum class EventType : uint8_t {
    NoteOn,
    NoteOff,
    PitchBend,
    ControlChange,
    ProgramChange,
    TransportPlay,
    TransportStop,
    TransportRecord,
    Clock,
    ParameterChange,
    PatchChange,
    SceneChange,
    AllNotesOff,
    Panic
};

struct SynthEvent {
    EventType type;
    uint8_t source;
    uint8_t channel;
    uint16_t id;
    int32_t value;
    uint32_t timestamp_us;
};
```

Adapt names and representation to repository conventions.

The same event path should eventually accept input from:

- USB MIDI
- arpeggiator
- sequencer
- UI
- automation
- diagnostic console
- future BLE MIDI
- future DIN MIDI

---

## 11. Concurrency

Use explicit task ownership.

A recommended initial ownership model is:

### Audio task

Owns:

- AMY rendering
- voice state
- DSP parameter application
- I²S buffer production
- underrun accounting

### Control task

Owns:

- application state
- patch selection
- macro evaluation
- controller mappings
- transport commands

### USB task

Owns:

- device enumeration
- USB MIDI reception
- disconnect and reconnect handling

### UI task

Owns:

- display drawing
- transient parameter pages
- diagnostics presentation

### Storage task or deferred worker

Owns:

- file access
- NVS
- preset persistence
- CRC validation

Do not share mutable structures between tasks without a defined ownership or synchronization model.

Prefer:

- single-writer ownership
- lock-free or bounded ring buffers
- short critical sections
- immutable snapshots for UI
- message passing

Avoid global mutable state.

---

## 12. C and C++ policy

Follow the language standard and style already used by the repository.

For new C++ code:

- use RAII where compatible with the embedded environment
- avoid exceptions unless already enabled and accepted
- avoid RTTI unless needed
- use fixed-width integer types for protocols and persisted formats
- validate all external input
- avoid implicit narrowing
- prefer `enum class`
- prefer explicit ownership
- avoid hidden heap allocation
- avoid large objects on small task stacks
- use `const` and `constexpr` where useful
- use names that express units, such as `sample_rate_hz`
- use timestamps with explicit units, such as `timestamp_us`

Do not introduce a formatting style that conflicts with the repository.

Do not reformat unrelated files.

---

## 13. Configuration policy

Hardware and product configuration must be centralized.

Do not scatter:

- GPIO numbers
- sample rate
- DMA sizes
- task priorities
- task stack sizes
- MIDI queue lengths
- display dimensions
- patch limits
- voice limits

Use the configuration mechanisms already adopted by the repository, such as:

- Kconfig
- build definitions
- central headers
- board configuration files

Board-dependent settings must not be embedded in generic drivers.

---

## 14. Unknown hardware details

The following must not be guessed:

- exact ESP32-S3 DevKitC revision
- exact native USB and USB-UART wiring
- final VBUS power circuit
- final GPIO map
- exact display controller
- display initialization sequence
- display color order
- display offsets
- PCM5102A module-specific strap configuration
- final audio connector and output stage

Where details are missing:

1. create a configurable interface
2. document the unresolved item
3. provide a safe placeholder or disabled default
4. do not represent the placeholder as final hardware design

---

## 15. Display rules

The display resolution is 284 × 76.

The UI must be designed specifically for a short panoramic screen.

Prioritize:

- high information density without clutter
- readable fonts
- bounded redraw regions
- low refresh overhead
- no audio-thread interaction
- transient parameter views
- MIDI and system diagnostics

Do not assume a display controller until confirmed.

The display subsystem must compile independently from the audio engine where practical.

A display failure must not prevent audio and MIDI from functioning.

---

## 16. Storage rules

Use persistent storage conservatively.

Do not write Flash for every knob movement.

Use:

- dirty flags
- deferred save
- inactivity timeout
- temporary file plus atomic replacement where supported
- CRC or equivalent integrity check
- versioned formats
- safe defaults
- compatibility checks

Persisted structures must not depend on compiler padding or raw pointers.

Use explicit serialization.

When a format changes:

- increment its version
- keep a migration path where reasonable
- reject incompatible data safely
- preserve the factory default patch

---

## 17. Diagnostics

Every hardware-facing subsystem must expose useful diagnostics.

At minimum track:

- audio underruns
- event queue overflows
- USB disconnects
- USB reconnects
- MIDI parse errors
- dropped MIDI events
- render maximum time
- free internal RAM
- free PSRAM
- active voices
- current sample rate
- current patch
- controller connection state

Diagnostics must not compromise real-time operation.

Rate-limit logs.

Do not print from an ISR or critical audio callback unless using a proven non-blocking mechanism.

---

## 18. Error handling

Never silently ignore initialization failures.

Each subsystem must return or expose a meaningful status.

Required degradation behavior:

- display failure: continue audio and MIDI
- storage failure: load embedded defaults
- USB absent: remain ready and retry
- MIDI disconnect: trigger Panic
- invalid patch: reject and load a safe patch
- audio start failure: stop dependent initialization and report clearly
- queue overflow: count, degrade, and preserve critical events
- PSRAM unavailable: report and use a reduced configuration if supported

Use safe failure states.

---

## 19. Panic and stuck-note prevention

Panic must clear:

- all active voices
- sustain state
- held-note state
- arpeggiator notes
- sequencer pending notes
- note ownership tables
- controller latch state where appropriate

Trigger Panic on:

- explicit user command
- USB MIDI disconnect
- USB Host reset
- controller profile replacement
- unrecoverable MIDI parser state
- relevant engine reset
- system recovery after task failure

A Note Off must never be treated as an unimportant event.

---

## 20. Build and test commands

Do not invent build commands.

Before using a command:

1. inspect `README`
2. inspect build files
3. inspect CI workflows
4. inspect scripts
5. identify the supported toolchain

Record the actual commands used in the change summary.

If the repository supports multiple platforms, avoid breaking non-ESP32 builds.

Run the narrowest relevant test first, then broader validation.

Typical validation categories are:

- host unit tests
- existing AMY tests
- formatting or lint checks
- ESP-IDF configuration
- ESP32-S3 build
- firmware size check
- hardware smoke test instructions

Only report a test as passed when it was actually run successfully.

Use one of these labels in the final report:

- `PASS`: executed and passed
- `FAIL`: executed and failed
- `NOT RUN`: not executed
- `BLOCKED`: could not execute because of a named dependency
- `MANUAL`: requires hardware verification

---

## 21. Testing expectations

Every change must include an appropriate validation strategy.

### Pure logic

Add automated tests for:

- MIDI parsing
- controller mapping
- macro curves
- soft takeover
- event priority
- patch serialization
- CRC validation
- arpeggiator timing
- sequencer step logic

### Audio integration

Provide tests or instrumentation for:

- buffer continuity
- underrun detection
- channel order
- zero initialization
- sample conversion
- clipping behavior
- render deadline

### USB integration

Provide manual or automated checks for:

- enumeration
- Note On and Note Off
- velocity zero handling
- rapid chords
- disconnect
- reconnect
- malformed or unexpected packets

### Hardware validation

When hardware is required, provide exact manual steps and expected observations.

Do not state that a hardware feature works when it has only compiled.

---

## 22. Performance changes

Measure before optimizing.

For performance-sensitive changes, report when possible:

- CPU load before and after
- maximum render time
- average render time
- memory before and after
- binary size change
- underrun count
- queue high-water mark
- latency implication

Avoid speculative micro-optimizations that reduce readability without measurements.

---

## 23. Change size

Prefer small, reviewable changes.

A good change should have:

- one primary purpose
- minimal unrelated edits
- clear acceptance criteria
- tests or a test plan
- no unnecessary dependency upgrades
- no broad formatting churn

If a task requires a large architectural change, split it into stages that leave the repository buildable.

---

## 24. Dependencies

Do not add a dependency before checking whether the repository or ESP-IDF already provides the required capability.

For every new dependency, document:

- purpose
- license
- version
- source
- maintenance status
- memory impact
- Flash impact
- real-time implications
- whether it is required or optional

Avoid dependencies that:

- allocate unpredictably
- require exceptions without project support
- impose a heavy UI framework prematurely
- perform blocking I/O in callbacks
- obscure critical hardware behavior

Pin dependency versions where the build system supports it.

---

## 25. Licensing

Preserve all existing copyright and license notices.

Before adding third-party code:

- verify license compatibility
- retain required notices
- identify modified files
- do not copy unlicensed snippets
- record the dependency in project documentation

Do not remove AMY attribution.

---

## 26. Documentation requirements

Update documentation when a change affects:

- build steps
- toolchain
- pin configuration
- hardware wiring
- task architecture
- persistent formats
- controller mapping
- test procedure
- limitations
- known issues

Documentation must distinguish:

- implemented behavior
- planned behavior
- experimentally validated behavior
- assumptions
- unresolved hardware details

---

## 27. Agent workflow

For each task:

1. restate the concrete objective internally
2. read the relevant specification section
3. inspect repository structure and current implementation
4. identify the smallest viable change
5. identify affected platforms
6. define acceptance criteria
7. implement
8. run relevant tests
9. inspect the diff
10. remove unrelated changes
11. summarize results and remaining risks

Before editing, identify:

- files likely to change
- real-time implications
- compatibility implications
- hardware assumptions
- test strategy

Do not make speculative edits across many modules.

---

## 28. Completion report

At the end of each task, report:

### Implemented

A concise list of actual changes.

### Architecture impact

How the change fits the system and whether it modifies AMY core.

### Validation

Commands and tests with status:

```text
PASS
FAIL
NOT RUN
BLOCKED
MANUAL
```

### Hardware assumptions

Any unresolved board, display, USB, VBUS, or PCM5102A details.

### Risks

Known limitations, regressions, or real-time concerns.

### Next smallest step

The next logical implementation step, without expanding the current task unnecessarily.

Do not claim completion if acceptance criteria remain unmet.

---

## 29. Definition of done

A task is done only when:

- the requested scope is implemented
- the repository remains buildable for the affected target
- relevant tests pass or are clearly marked
- no unrelated changes remain
- no final hardware pinout was invented
- real-time constraints were respected
- documentation is updated where needed
- errors and limitations are reported honestly
- the diff is understandable to another engineer

Compilation alone is not proof of functional hardware behavior.

---

## 30. Initial project milestone

Until the first milestone is complete, prioritize only this path:

```text
SMK25 V2
    ↓ USB MIDI Host
ESP32-S3
    ↓ AMY
I²S
    ↓
PCM5102A
    ↓
stereo line output
```

The first milestone requires:

- reproducible ESP32-S3 build
- PCM5102A stereo test tone
- stable DMA audio
- AMY patch rendering
- USB MIDI device detection
- Note On
- Note Off
- velocity
- pitch bend
- modulation
- automatic Panic on disconnect
- reconnect without full reboot where feasible
- diagnostic counters
- documented hardware test procedure

Do not prioritize the sequencer, scenes, elaborate UI, BLE MIDI, or editor tooling before this milestone is stable.

---

## 31. Final rule

Never trade audio stability for feature count.

When uncertain, choose the design that is:

- simpler
- bounded
- measurable
- reversible
- testable
- compatible with upstream AMY
