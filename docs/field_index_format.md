# Field index format

TODO (phase 1). Precise binary layout of `swiftfix::FieldIndex` as it
crosses from the pre-parser into patched QuickFIX.

Planned contents:

- Byte-level layout of `FieldEntry`, `FieldIndex`.
- Invariants the scanner guarantees (e.g. entries sorted by position).
- Lifetime/ownership model (thread-local arena vs. caller-provided buffer).
- How header fast-access slots (`begin_string_idx`, …) index into `fields`.
- Endianness notes (we're little-endian only for phase 0).
