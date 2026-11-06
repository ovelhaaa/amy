# Output 0.8 gap classification added for 0.9

The preserved0.8 capture records145 fallback blocks and66 observed generation
barriers. It cannot assign the remaining79 individually. Those counts are not
reclassified retrospectively. Zero I2S errors does not prove continuous PCM.

The independent output task now publishes mutually exclusive fallback buckets:
`boot`, `patch`, `panic`, `stale`, `unexpected_active`, `other`, plus the separate
count of discarded stale PCM blocks. Their sum equals `silence_blocks` for fallback
production. A stale block followed by a valid block increases discarded count
without increasing fallback. Ordinary valid PCM containing musical silence is
not inspected or counted as an underrun. Worker failure zeros are not claimed
as healthy musical audio; patch FAILED is exposed separately.

`output_context` atomically publishes generation, owner reason and occupied-gate
flag. `transition_context` preserves reason for generation barriers even after the
owner returns READY. The initial unprimed READY output window before its first valid PCM is classified
BOOT. Boot/Patch/Panic use explicit causes; output never calls AMY.
Mismatched generation/context is classified OTHER, not guessed. Matching boot,
patch or Panic contexts take precedence over stale and READY-active starvation.
READY with occupied gates, no data, no transition/stale explanation increments
UNEXPECTED_ACTIVE. READY idle, failed or ambiguous windows increment OTHER.
Release-tail-only starvation is OTHER because musical gate count can be zero.
Contexts retain28 generation bits; this is diagnostic tagging only, not the full
transport generation check used to admit PCM/commands.

All counters are output-owner local and copied using the existing metrics mailbox.
No output allocation, log, mutex, wait or increased queue. GAP09 rate-limited
logging runs on the existing diagnostics/UI side. Tests independently inject all
reason classes, generation mismatch and stale handoff blocks.

Gate0 software instrumentation: PASS. Old unexplained blocks: unresolved.
Gate0 physical observation/cause correction: MANUAL / NOT RUN. No evidence was
available to demonstrate or correct a sustained READY-active starvation cause;
no speculative scheduling refactor was made. Musical software was implemented
and tested for review, but experimental physical readiness remains conditional
on the short smoke below. This is not a real-time Gate F PASS.

Quick check: capture GAP09/OUTPUT08/HARMONY09/PATCH08 counters immediately before
and after boot, one Panic, one FM/Juno selection, and30-60s sustained playback
with Harmony OFF then two triads. Attribute deltas, not cumulative totals. If
UNEXPECTED_ACTIVE grows during sustained READY play, stop musical qualification,
record patch/gates/render+compute maxima and reproduce a specific cause before
promoting this candidate. OTHER growth needs inspection; never assume intentional.
No automatic thirty-minute window is required. DMA hardware underruns and analog
gaps remain unmeasured without capture/instrumentation.
