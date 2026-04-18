# Fallback policy

TODO (phase 3). When and how we fall back from the pre-parser to stock
QuickFIX parsing.

Planned contents:

- Per-`ScanStatus` decision table.
- What state the caller must reset before retrying via the stock path.
- Diagnostics: metrics/counters the shim exposes for observability.
- CI requirement: with the feature flag off, behavior is byte-identical to
  vanilla QuickFIX on every corpus frame.
