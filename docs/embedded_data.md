# Embedded data handling

TODO (phase 2). Strategy for FIX fields where `<SOH>` may legally appear
inside the value — specifically length-prefixed pairs like (95, 96).

Planned contents:

- Which tag pairs in FIX.4.4 / FIX.5 are length-prefixed.
- How the scalar scanner reads the length tag to know how many bytes to
  skip.
- SIMD challenge: the AVX2 "find all <SOH>" kernel has to be told to
  ignore some of its matches. Proposed solution + alternatives.
- Fallback trigger: when in doubt, return `ScanStatus::FallbackRequested`
  and let stock QuickFIX handle it.
