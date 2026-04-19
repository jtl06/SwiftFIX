# Field index format

Binary layout of `swiftfix::FieldIndex` as it crosses from the pre-parser
into patched QuickFIX. This is the authoritative spec; the struct
definitions in `preparser/include/swiftfix/field_index.hpp` must match.

## Status

Phase 1 draft. Entry packing and invariants are locked; ownership
(caller-provided buffer vs. thread-local arena vs. caller-owned
`FieldIndex` reused via `reset()`) is still open and marked so below.

## FieldEntry — 16 bytes

One entry per scanned field in the frame, in the order the fields appear
on the wire. All offsets are relative to the start of the original raw
FIX buffer the scanner was called with; they are **not** bounds into any
scratch copy.

```
 Offset  Size  Field         Notes
 ------  ----  -----------   ----------------------------------------
   0      4    tag_start     first byte of the tag digits
   4      4    value_start   byte after the '=' (first value byte)
   8      4    value_end     offset of the <SOH> terminating the value
  12      4    tag_number    parsed tag as uint32 (e.g. 35 for MsgType)
```

Size: `4 × uint32_t = 16 B`. `static_assert` in the header locks this.

### Why not store `equals_pos`?

The consumer's hot path is `QuickFIX::Message::setString`-equivalent
work: copy `[value_start, value_end)` as the value, dispatch on
`tag_number`. `equals_pos` is only interesting *during* scanning and is
trivially `value_start - 1` if ever needed. Dropping it buys us the
16-byte packing (4 entries per cache line) and removes a redundant
derivable field.

### Why uint32?

- 32-bit offsets cap a single FIX frame at 4 GiB. Real frames are
  hundreds of bytes.
- Packs to 16 B cleanly; 16-bit offsets would shave another 4 B per
  entry but force us to fail large RawData frames that are otherwise
  valid.
- `tag_number` stays uint32 because custom-tag namespaces occasionally
  exceed uint16 and the cost is zero given we're padding to 16 B
  regardless.

## FieldIndex — 4124 bytes at `kMaxFields = 256`

```
 Offset  Size  Field                       Notes
 ------  ----  --------------------------  -------------------------------
    0     4    begin_string_idx  (i32)     index into fields[] for tag  8, or -1
    4     4    body_length_idx   (i32)                                   9,     -1
    8     4    msg_type_idx      (i32)                                  35,     -1
   12     4    checksum_idx      (i32)                                  10,     -1
   16     4    declared_body_length (u32)  value of tag 9, in bytes
   20     4    frame_length         (u32)  total bytes consumed including trailing <SOH>
   24     4    field_count          (u32)  populated entries in fields[]
   28  4096    fields[256] (FieldEntry)    16 B * 256
 -----------
 total  4124
```

Header slots (`*_idx`) are **indices into `fields`**, not offsets into
the raw buffer. `-1` means "tag not yet seen / not found." Consumers
that need the offset go through `fields[begin_string_idx].tag_start`
etc.

## Invariants the scanner must uphold on `ScanStatus::Ok`

1. `field_count <= kMaxFields`.
2. `fields[0 .. field_count)` are sorted by `tag_start` ascending
   (wire order — never rearranged, including for repeated tags).
3. For every populated `FieldEntry e`:
   - `e.tag_start < e.value_start - 1 < e.value_end < frame_length`.
   - The byte at `e.value_start - 1` is `'='` (0x3D).
   - The byte at `e.value_end` is `<SOH>` (0x01).
   - The tag digits in `[e.tag_start, e.value_start - 1)` parse to
     `e.tag_number` with no leading zeros (except the single digit "0"
     which FIX forbids anyway — if seen, status is `Malformed`).
4. `begin_string_idx`, `body_length_idx`, `msg_type_idx`,
   `checksum_idx` each point to the *first* occurrence of their tag, or
   are `-1`. On `Ok` the first three are required to be non-`-1` and in
   the order `begin_string_idx < body_length_idx < msg_type_idx`;
   otherwise status is `BadHeader`, not `Ok`.
5. `declared_body_length` equals the parsed value of tag 9. The
   equality `frame_length == (fields[body_length_idx].value_end + 1) +
   declared_body_length + /* "10=xxx<SOH>" */ 7` must hold; otherwise
   status is `BadBodyLength`.
6. `frame_length` counts through and including the `<SOH>` that
   terminates the checksum field.

On non-`Ok` status, `FieldIndex` may be partially populated and
**must not be read by the caller.** The caller resets it or discards
it before retrying via the fallback path.

## Endianness

Little-endian only in phase 0. The format is an in-process handoff
between two C++ modules compiled for the same target — no network
byte-order concerns. If/when a cross-process IPC boundary appears, this
doc gets a `Serialization` section.

## Lifetime & ownership

**Caller-owned, reused.** The `Scanner::scan(buffer, FieldIndex& out)`
signature is the contract: callers pass a `FieldIndex` they own, scanner
calls `out.reset()` and repopulates. Zero allocation in steady state.

The shim's integration point is `swiftfix::shim::SessionShim`, which
holds one `FieldIndex` per session (matching QuickFIX's per-session
`FIX::Parser` model). Multi-threaded consumers instantiate one
`SessionShim` per session/reader thread — no shared mutable state.

**Ruled out:** library-owned `thread_local` or singleton-held storage.
Returning a reference that's invalidated on the next `scan()` has no
precedent in QuickFIX (its result surfaces are all caller-owned
objects) and turns re-entrancy mistakes into silent data corruption
instead of perf regressions. Tests and bench harnesses that want two
indexes live at once can trivially have them with caller-owned; they
can't with thread-local.

The SIMD path can bake the same assumption: `scan(span, FieldIndex&)`
is the canonical signature across scanner kinds.

## Changes that require a new format version

- Any change to `FieldEntry` size, field order, or semantics.
- Any change to `FieldIndex` header (adding/removing fast-access slots,
  changing their meaning).
- Changing `kMaxFields` *is not* a format change — it's a capacity knob
  the shim observes via `fields.size()`.
