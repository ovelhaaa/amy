# Harmony 0.9 architecture

Explicit P4 request supersedes the older S3/284x76 specification for this application.
Baseline: `feature/smk-p4-unified-patch-engine-08` at `f40f34b4`.
Isolated worktree: `build/worktrees/smk-p4-harmony-assist-09`.
No shared AMY `src/`, S3 code, driver, electrical assignment, dependency version,
master gain, FM/Juno algorithm, effects or voice count was changed.

## Flow and task ownership

USB packet -> existing USB64 SPSC queue -> synthesis owner -> input identity ->
`harmony::transform` -> atomic group reservation -> eight existing AMY instruments.
Touch -> existing UI32 SPSC queue + release latches -> same owner and allocator.
No generated note is put back into either input queue. No second transform.
Independent priority-6 PCM output remains separate from priority-4 synthesis.
UI consumes the existing copied snapshot; no direct writes into owner structures.

`amy_runtime/harmony.h` has only fixed integers, fixed arrays and bounded loops;
it imports neither AMY, IDF, LVGL nor USB. P4 preview uses this same implementation.
The existing S3 utilities were reviewed: their enum order differs, ChordMemory
uses a vector, and both utilities are marked experimental/unwired. They are not
used in P4 and remain unchanged. The old P4 preview scale table was removed.
P4 pentatonic-major mask corrected from 0x2a5 (F) to 0x295 (E), independently
checked against the actual degrees 0,2,4,7,9. This is an intentional correction
of the inherited preview/S3 discrepancy, not an accidental third definition.

## Mathematics and boundaries

Eleven Blueprint scales in their original order: chromatic, major, natural minor,
harmonic minor, major/minor pentatonic, blues, Dorian, Phrygian, Lydian, Mixolydian.
Quantizer modes 0/1 are nearest with ties up/down (existing names retained), 2
snaps an invalid note to the tonic of its octave (one octave down if >127), 3
filters invalid notes, 4 always down, 5 always up. At MIDI endpoints a directional
mode falls back to the only available direction if none exists on its preferred
side. Valid notes are unchanged. Scale OFF is identity before chord processing.

Chord IDs 0..9: off, major, minor, diminished, augmented, sus2, sus4, dominant7,
major7, minor7. IDs 10/11 (Add9/9th) stay planned and are rejected. IDs 12/13:
diatonic triad/seventh, ascending scale degrees 1/3/5[/7]. Only seven-note scales
are accepted for diatonic mode. With Scale OFF an out-of-scale root is rejected;
no absent degree is invented. Configuration remains applied; the note result
reports INVALID. Third inversion is valid only for tetrads. CLOSE/OPEN available;
DROP2/SPREAD/POWER voicing choices remain planned. OPEN raises the second sorted
tone one octave; numeric Spread adds 0..2 further octaves to that same tone.
Sorting and deduplication are deterministic. An over-range chord is moved down
as a complete group by the smallest whole number of octaves needed; individual
tones are never wrapped/clamped/dropped. An impossible range rejects the group.

## Configuration protocol and UI

A complete 27-bit configuration travels in a generation-tagged Harmony command.
Owner validates it, publishes the applied configuration and increments its
revision. UI admits one outstanding Harmony request, waits for a matching owner
revision/configuration, and times out after 2s without pretending it applied.
Existing groups freeze their creation configuration and pitches. No global
Panic for a scale/chord change. Patch loading retains both generation barriers.

Safe boot: Scale OFF, Chord OFF, USB channel1, Touch eligible but transformed
only when a musical feature is enabled. Channel1..16 is a Harmony eligibility
filter: other channels still play directly and keep their bend/sustain identity.
Touch eligibility is independent. Five atomic RAM presets: normal, C major
triads, A natural minor triads, C major sevenths, C-major scale only. They preserve
source eligibility/channel. No NVS or last-setting persistence.

Only implemented controls are AVAILABLE in the Harmony profile. Unsupported
chord/voicing values are rejected by the adapter and skipped by live steppers.
Accidentals, velocity curves and Layer B/destination remain PLANNED. All generated
notes receive original velocity. Existing single-pass per-slot bend and CC1
software vibrato naturally apply to every USB member of a group on that channel.

Harmony keyboard issues one original Touch On, preserving gesture token/lease
semantics. Inspector explicitly separates PREDICT from LAST ADMITTED/FILTERED/
POLYPHONY/INVALID/NONE. Applied/requested configuration, occupied/capacity, rejection
and error counters are owner-derived. Historical LAST ADMITTED is not a claim
that the group is still sounding. Keyboard colors show prediction; they do not
claim admission. Parameter labels refresh after owner confirmation.

## Resource bounds

Eight group entries, four generated notes per group, eight physical voice slots.
No sharing/refcounting or stealing. Groups use independent physical voices even
when pitches overlap. Input queues retain USB64/Touch32; owner drains at most32
commands from each per block. No musical malloc/free or locks added. Harmony
profile preallocates6072 delta nodes at boot (three2048-node AMY blocks), versus
3000 (two blocks) in0.8, to cover four-note Touch retrigger bursts. Existing
boot admission includes actual target ABI delta size. Additional static ledger
and snapshot sizes are recorded separately from heap. Timed transform/admission
maxima are exposed via HARMONY09 on the non-audio diagnostic task.

No claim of measured P4 latency or clean analog audio follows from host timing.
