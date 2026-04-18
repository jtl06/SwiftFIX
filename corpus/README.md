# Test corpus

Raw FIX messages used by tests, benchmarks, and the scalar-vs-SIMD
equivalence check (once that lands in phase 2).

## On-disk format

One FIX frame per file. Raw bytes — **do not substitute <SOH> (`0x01`) with
any visible placeholder**. The bench harness and tests read these bytes
verbatim and feed them into QuickFIX.

Files with no extension are the convention. `.bin` is acceptable.

## Directory layout

| Directory            | What's in it                                          |
|----------------------|-------------------------------------------------------|
| `valid/`             | Well-formed frames every scanner must accept          |
| `truncated/`         | Frames cut off mid-tag / mid-value                    |
| `bad_checksum/`      | Header looks fine, tag 10 (CheckSum) is wrong         |
| `bad_bodylength/`    | Tag 9 value disagrees with actual body byte count     |
| `repeated_tags/`     | Same tag present multiple times (valid for groups)    |
| `rawdata/`           | Embedded-binary edge cases (tag 95/96). See           |
|                      | `docs/embedded_data.md`                               |

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
