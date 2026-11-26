# Harmonic Pads 0.19-A contracts

The explicit P4/800x480 request supersedes the legacy S3 specification for this application. Baseline: ff2f098d0d6417273198e4faed782fe6ad6a4b32. AMY DSP and S3 integration remain unchanged.

## Audit and decision

Touch identity is (UI generation, input pitch, token), with pitch-indexed Off latches and a nonrenewable ten-second safety lease. It cannot represent independent pads or persistent HOLD safely. Pads must not issue multiple Touch Ons.

The existing audio owner remains the sole allocator. A dedicated HarmonicPad origin identifies groups by pad index, owner generation and a monotonically allocated UI token. The pad index is independent of generated pitch. A release latch per pad survives queue saturation; a stale Off cannot release a later token. Release-all writes the eight current tokens, never a global Panic. No second allocator or pitch sharing is introduced.

## Musical flow

Local pad configuration defaults to C major, octave 4 (MIDI C4=60). Only seven-note masks are supported. Resolve the ascending scale degree from harmony::kScales, with pad eight one octave above tonic. Request transports the input fundamental and frozen validated harmony::Config (diatonic triad, optional supported inversions/voicing for contract tests). Owner invokes harmony::transform exactly once and uses its existing atomic reservation. Global Harmony eligibility and configuration are bypassed for this origin, never modified. Existing groups retain notes and configuration when local controls change. Whole-group folding is inherited from Harmony; invalid input is rejected.

## Ownership and lifecycle

UI requests and owner admissions are distinct. Per-pad snapshot includes request token, generation, result and active token; visual ACTIVE requires matching owner confirmation. Rejection creates no group. USB disconnect invalidates only USB ownership. Keyboard Touch, Arp and Sequencer retain their existing contracts. Patch/Panic generations invalidate queued pad commands and clear their groups. Screen teardown and RELEASE ALL PADS release only matching pad tokens.

MOMENTARY uses PRESSED once, RELEASED and PRESS_LOST as Off; CLICKED never sends On. DELETE releases owned tokens. HOLD toggles on PRESSED; release does not stop it. Switching execution modes releases all pad groups. HOLD requires a separate renewable UI heartbeat lease; it must not inherit Touch's fixed ten-second lease. UI loss expires the session on the owner, and already queued stale requests cannot resurrect it. Ordinary navigation releases pads; HOME never calls a USB/global release.

## Resource and real-time acceptance

Eight fixed status records, eight atomic release latches and one session heartbeat; no queue capacity increase. Command configuration payload and snapshot deltas must be measured using sizeof on host and target build reports. Admission scans only the existing eight groups/voices and uses existing bounded Harmony loops. No allocation, locks, logging, storage or display operations occur in musical processing. UI allocation remains within the existing 128 KiB LVGL pool. Timing evidence is host-only until physical qualification.

## Validation gates

Navigation protection is committed independently first. Pure musical cases cover all heptatonic masks, names, MIDI limits and voicings. Real owner cases cover shared pitches, stale tokens, capacity, queue pressure, USB isolation, generation barriers, heartbeat expiry and frozen configuration. Real LVGL tests must exercise lifecycle, navigation and stress, inspecting owner gates rather than widget disappearance. Firmware builds and resource comparison remain mandatory. No flashing or serial monitoring is authorized.

## Future 0.19-B (planning only)

Transport-neutral pad intent can later accept confirmed SMK25 mappings, optional Arp routing and configurable chord banks. Scene persistence, physical-pad mappings and progression playback are outside 0.19-A.
