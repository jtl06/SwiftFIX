# SwiftFIX design

Living design document. Sections that are TODOs are expected to stay TODOs
until the phase that resolves them — see the Milestones section for the
phasing.

## Goals

TODO — what we're trying to accomplish. Cover: pre-parser role relative to
QuickFIX, the specific ingestion hotspot we're targeting, the wall-clock
objective (e.g. "reduce p99 inbound message decode by Xµs on corpus Y").

## Non-Goals

TODO — what we're explicitly not doing. Cover: no full FIX engine; no
session management; no message mutation; no semantic validation; no
FIXT/FIX5 yet; no outbound encoding; no Windows support in phase 0.

## Phase 0 Findings

TODO — to be filled in after `scripts/profile.sh` runs against the corpus.
Cover: what percentage of stock-QuickFIX CPU time is in scan/tokenize
paths, per-message cycle budget, the identifiable hotspots in `Message.cpp`
and `Parser.cpp`, and whether the numbers justify the rest of this plan.

## Field Index Format

TODO — precise binary layout the pre-parser hands QuickFIX, including
alignment requirements, lifetime, zero-copy guarantees, and how header
fast-access slots relate to the generic `fields` array. See
`docs/field_index_format.md` for the detailed spec once drafted.

## QuickFIX Patch Surface

TODO — list the specific QuickFIX functions the patch series modifies,
what each modification does, and what the un-patched behavior is when the
preparse feature flag is off. Reference each patch under
`integration/patches/` by filename.

## Fallback Policy

TODO — enumerate every `ScanStatus` that triggers a fallback and the
stock-QuickFIX code path we fall back to in each case. See
`docs/fallback_policy.md` for the detailed spec.

## Embedded Data Strategy

TODO — how we handle tag 95 (RawDataLength) / 96 (RawData) and other
length-prefixed embedded regions where `<SOH>` is legal inside the value.
See `docs/embedded_data.md`.

## Milestones

TODO — phased plan:

- **Phase 0 (scaffold, profile):** prove the scan path is hot. This phase.
- **Phase 1 (scalar scanner):** reference impl; passes corpus; matches
  QuickFIX semantics for every message.
- **Phase 2 (SIMD scanner):** AVX2 fast path with runtime dispatch;
  scalar-vs-SIMD equivalence gate in CI.
- **Phase 3 (shim + patches):** wire into QuickFIX behind feature flag;
  end-to-end benchmarks show wall-clock improvement.
- **Phase 4 (harden, ship):** fuzz, long-running soak, cross-compiler, docs.

## Success Criteria

TODO — quantitative targets per phase. At minimum: p99 and mean speedup
on the `corpus/valid/` workload relative to the baselines under
`bench/baseline/`, with the preparse feature flag enabled vs. off.

## Risks

TODO — what could break the plan. Known candidates: scan path isn't
actually hot; QuickFIX API surface resists clean patching; RawData edge
cases force scalar fallback frequently enough to erase SIMD wins;
runtime-dispatch overhead dominates the saved cycles for short messages.
