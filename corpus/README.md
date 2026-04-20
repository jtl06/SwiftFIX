# Test corpus

Raw FIX messages used by tests, benchmarks, and the scalar-vs-SIMD
equivalence check (once that lands in phase 2).

## On-disk formats

Two shapes, depending on use:

- **Per-file corpora** (`valid/`, `truncated/`, `bad_*`, etc.): one FIX
  frame per file, raw bytes. Tests and correctness checks use these —
  one input file = one expected parse result.
- **Stream corpus** (`bulk.stream`): many frames concatenated back-to-back
  into a single binary blob, matching how FIX looks on the wire
  (continuous TCP bytes, messages demarcated by `8=.../9=<n>/10=cksm`).
  The bench harness feeds this through `FIX::Parser`, which is the stock
  stream-splitting path.

**Do not substitute <SOH> (`0x01`) with any visible placeholder** in
either shape. The tools read bytes verbatim.

Files with no extension are the convention for per-file corpora. `.bin`
is acceptable.

## Directory layout

| Path                  | What's in it                                          |
|-----------------------|-------------------------------------------------------|
| `valid/`              | Well-formed frames every scanner must accept          |
| `truncated/`          | Frames cut off mid-tag / mid-value                    |
| `bad_checksum/`       | Header looks fine, tag 10 (CheckSum) is wrong         |
| `bad_bodylength/`     | Tag 9 value disagrees with actual body byte count     |
| `repeated_tags/`      | Same tag present multiple times (valid for groups)    |
| `rawdata/`            | Embedded-binary edge cases (tag 95/96). See           |
|                       | `docs/embedded_data.md`                               |
| `bulk.stream` *(file)*| Concatenated procedurally-generated frames for bench  |
| `bulk25k.stream` *(file)*| 25k-message higher-variance stream (~5.77 MB, exceeds L2) |

## Generating the canonical `valid/` set

`generate.py` rebuilds `valid/` from scratch using the synthetic templates
it defines inline. The generator is the authority on message content —
edit the templates there, then re-run:

```
python3 corpus/generate.py
```

This recomputes tag 9 (`BodyLength`) and tag 10 (`CheckSum`) so every
written frame is well-formed. Commit both `generate.py` and the regenerated
bytes so the corpus is reproducible from the script alone.

## Generating the bulk stream

```
python3 corpus/generate.py --bulk 1000 --seed 42
```

Writes `corpus/bulk.stream` — N procedurally-varied frames concatenated
into one file, using a seeded RNG so runs are reproducible. Point the
bench at it:

```
SWIFTFIX_CORPUS=corpus/bulk.stream scripts/bench.sh
```

The harness auto-detects: if `SWIFTFIX_CORPUS` is a file, it runs the
stream-parse benchmark (`FIX::Parser` does the splitting); if it's a
directory, it runs the per-file benchmark (already-split frames fed
straight into `Message::setString`).

A larger, higher-variance stream lives at `bulk25k.stream` — 25,000
messages (~5.77 MB, exceeds L2), with News (`35=B`) and MD-Incremental
(`35=X`) added for longer free-text / deep-book variance. Regenerate
with:

```
python3 corpus/generate.py --bulk 25000 --seed 3203386110 \
    --bulk-stream corpus/bulk25k.stream
```

Note: stock QuickFIX's `FIX::Parser` scales poorly past a few MB
(memmoves on each dequeue make splitting effectively O(N²) in stream
size), so the QuickFIX baseline on this corpus isn't apples-to-apples
with the SwiftFIX scanners. Use `bulk.stream` for clean scanner-vs-QF
comparisons.

## Adding new messages

1. **Valid frames:** add a template to `generate.py` and re-run. This
   guarantees BodyLength and CheckSum stay correct.
2. **Malformed frames:** write the file by hand (or start from
   `generate.py` output and mutate). Put it in the appropriate
   `bad_*` / `truncated/` directory and add a sibling `.notes.txt`
   explaining *what* is wrong with the frame.

## Provenance

These messages are synthetic — hand-crafted from the FIX.4.4 specification,
not captured from any real session. No sensitive data.
