# Perf log

Per-change numbers for `SwiftFIX_ScalarSplit` on `corpus/bulk.stream` (1190 msgs, i7-12650H P-core pin, Release, CPU scaling on). Run via `make bench-perf`. Per-message counters = per-iter ÷ 1190.

---

## Reference — QuickFIX (commit `42ce312`)

Stock `FIX::Parser` on the same corpus. Here as the comparison baseline.

| Metric            | `StreamSplit` | `StreamParse` |
|-------------------|---------------|---------------|
| p50 time          | 1.04 ms       | 1.91 ms       |
| Per msg           | 874 ns        | 1,605 ns      |
| Throughput        | 146 MiB/s     | 80 MiB/s      |
| Msg/s             | 959 k         | 525 k         |
| Instructions/msg  | 722           | —             |
| Cycles/msg        | 3,999         | —             |
| Branches/msg      | 168           | —             |
| L1-D misses/msg   | 911           | —             |
| IPC               | 0.18          | —             |

---

## Baseline — initial scalar scanner (commit `42ce312`)

| Metric            | Value       |
|-------------------|-------------|
| p50 time          | 123 µs      |
| Per msg           | 103 ns      |
| Throughput        | 1.21 GiB/s  |
| Msg/s             | 8.12 M      |
| Instructions/msg  | 1,788       |
| Cycles/msg        | 473         |
| Branches/msg      | 453         |
| L1-D misses/msg   | 0.76        |
| IPC               | 3.78        |

---

## 2026-04-19 — pointer walk in `scan_one_field`

Replaced cursor/index arithmetic in `scan_one_field` with a `const unsigned char*` walk. Offsets computed once at the end.

| Metric            | Before  | After   | Δ          |
|-------------------|---------|---------|------------|
| p50 time          | 123 µs  | 121 µs  | -2%        |
| Per msg           | 103 ns  | 102 ns  | -2%        |
| Throughput        | 1.21 GiB/s | 1.227 GiB/s | +1.4% |
| Msg/s             | 8.12 M  | 8.26 M  | +1.7%      |
| Instructions/msg  | 1,788   | 1,761   | -1.5%      |
| Cycles/msg        | 473     | 458     | -3.2%      |
| Branches/msg      | 453     | 453     | flat       |
| L1-D misses/msg   | 0.76    | 0.21    | -72%       |
| IPC               | 3.78    | 3.85    | +2%        |

---

## 2026-04-19 — special-case fixed header (tags 8 / 9 / 35)

Single ≥12-byte buffer precheck, then literal `"8="` / `"9="` / `"35="` matches. Tag-9 digit parse fused with its SOH scan (drops `parse_uint` call on the hot path).

| Metric            | Before  | After   | Δ          |
|-------------------|---------|---------|------------|
| p50 time          | 121 µs  | 116 µs  | -4%        |
| Per msg           | 102 ns  | 97 ns   | -4%        |
| Throughput        | 1.227 GiB/s | 1.275 GiB/s | +3.9% |
| Msg/s             | 8.26 M  | 8.59 M  | +4.0%      |
| Instructions/msg  | 1,761   | 1,706   | -3.1%      |
| Cycles/msg        | 458     | 439     | -4.1%      |
| Branches/msg      | 453     | 428     | -5.5%      |
| L1-D misses/msg   | 0.21    | 0.33    | +0.12      |
| IPC               | 3.85    | 3.89    | +1%        |

---

## 2026-04-19 — `[[unlikely]]` on cold branches

Annotated error/truncation returns in `scan`, `scan_one_field`, `parse_uint` and the switch cases for `FieldScan::Truncated/Malformed`. Compiler moves cold code out of the hot path.

| Metric            | Before  | After   | Δ          |
|-------------------|---------|---------|------------|
| p50 time          | 116 µs  | 99.4 µs | -14%       |
| Per msg           | 97 ns   | 83.5 ns | -14%       |
| Throughput        | 1.275 GiB/s | 1.497 GiB/s | +17% |
| Msg/s             | 8.59 M  | 10.08 M | +17%       |
| Instructions/msg  | 1,706   | 1,373   | -20%       |
| Cycles/msg        | 439     | 374     | -15%       |
| Branches/msg      | 428     | 380     | -11%       |
| L1-D misses/msg   | 0.33    | 0.04    | ~0         |
| IPC               | 3.89    | 3.67    | -6%        |

---

## 2026-04-19 — pointer-based `scan_one_field` API

Replaced `(span, size_t& cursor)` with `(const unsigned char*& p, end, base)`. Drops per-call `reinterpret_cast` + `base + cursor` + `cursor = p - base`. `scan()` body loop fully pointer-native via `body_end_ptr`.

| Metric            | Before  | After   | Δ          |
|-------------------|---------|---------|------------|
| p50 time          | 99.4 µs | 94.8 µs | -5%        |
| Per msg           | 83.5 ns | 79.7 ns | -5%        |
| Throughput        | 1.497 GiB/s | 1.568 GiB/s | +4.7% |
| Msg/s             | 10.08 M | 10.56 M | +4.8%      |
| Instructions/msg  | 1,373   | 1,417   | +3%        |
| Cycles/msg        | 374     | 357     | -5%        |
| Branches/msg      | 380     | 381     | flat       |
| L1-D misses/msg   | 0.04    | 0.05    | ~0         |
| IPC               | 3.67    | 3.97    | +8%        |

---

## 2026-04-19 — AVX2 SOH scan (`SwiftFIX_Avx2Split`)

New `Avx2Scanner` compiled alongside the scalar path; runtime dispatch via `__builtin_cpu_supports("avx2")`. Replaces the byte-by-byte `while (*p != 0x01)` SOH scan in tag 35 and `scan_one_field` with a 32-byte AVX2 compare (`_mm256_cmpeq_epi8` + `movemask` + `countr_zero`), falling back to the scalar loop for the tail. New dedicated bench `SwiftFIX_Avx2Split`; previous rows were the scalar path measured via `default_scanner()`, which is why the scalar column below matches historical numbers.

| Metric            | Scalar  | AVX2    | Δ          |
|-------------------|---------|---------|------------|
| p50 time          | 96.8 µs | 77.9 µs | -20%       |
| Per msg           | 81.4 ns | 65.5 ns | -20%       |
| Throughput        | 1.535 GiB/s | 1.906 GiB/s | +24% |
| Msg/s             | 10.34 M | 12.83 M | +24%       |
| Instructions/msg  | 1,417   | 1,114   | -21%       |
| Cycles/msg        | 367     | 295     | -20%       |
| Branches/msg      | 381     | 221     | -42%       |
| L1-D misses/msg   | 0.065   | 0.066   | ~flat      |
| IPC               | 3.86    | 3.77    | -2%        |

---

## 2026-04-19 — AVX2 on tag side too (regression, reverted)

Tried widening AVX2 from value-SOH-only to a combined `=` + SOH scan inside `scan_one_field`, plus header tags 8 and 35 also routed through `find_soh_avx2`. The combined scan loaded 32-byte (and at one point 16-byte fallback) windows looking for both delimiters at once so the tag-digit loop could be range-based. **Regressed** — reverted to the prior shape (scalar tag walk, scalar headers, AVX2 only on the value SOH).

| Metric            | AVX2 (SOH-only) | AVX2 (combined) | Δ          |
|-------------------|-----------------|-----------------|------------|
| Per msg          | 65.5 ns         | 100.2 ns        | +53%       |
| Throughput        | 1.906 GiB/s     | 1.246 GiB/s     | -35%       |
| Instructions/msg  | 1,114           | 1,247           | +12%       |
| Cycles/msg        | 295             | 449             | +52%       |
| Branches/msg      | 221             | 242             | +10%       |
| L1-D misses/msg   | 0.066           | 0.219           | +3.3×      |

Why it lost: tags are 1–4 bytes typical, so a 32-byte SIMD scan over the tag region reads ahead into bytes the tag walk would never have touched (extra L1 traffic) and the bookkeeping (`any_mask`, first-bit detect, `soh_after_eq` shift, plus a scalar tag-digit pass on the bytes the SIMD already loaded) costs more cycles than the byte-by-byte tag walk it replaced. The wide value scan stays a win because typical FIX values are tens to hundreds of bytes — well past the SIMD breakeven.

---

## 2026-04-19 — direct 3-byte digit check for tag 10

FIX checksum is exactly 3 ASCII digits. Replaced the `parse_uint` call (loop, overflow guard, output write into a `[[maybe_unused]]` local) with three unrolled `(v[i] - '0') > 9u` compares. Applied to both scanners; `parse_uint` deleted from both TUs.

| Metric            | Scalar (before) | Scalar (after) | AVX2 (before) | AVX2 (after) |
|-------------------|-----------------|----------------|---------------|--------------|
| p50 time          | 96.6 µs         | 100.0 µs *     | 75.8 µs       | 76.1 µs      |
| Per msg           | 81.2 ns         | 84.0 ns *      | 63.7 ns       | 63.9 ns      |
| Instructions/msg  | 1,417           | 1,387          | 1,140         | 1,110        |
| Cycles/msg        | 362             | 376 *          | 289           | 285          |
| Branches/msg      | 381             | 374            | 236           | 229          |

\* Time/cycle deltas on the scalar column are within run-to-run noise (CV ~1.5%); instructions and branches drop is the real measured change. AVX2 dropped 30 ins/msg and 4 cyc/msg — small, free, no behavioral change since QuickFIX still validates the checksum value.

---

## 2026-04-19 — whole-message SOH indexing

Replaced the per-field `scan_one_field` (which did its own AVX2 SOH scan for each field's value) with a two-pass shape: one bulk AVX2 pass over `[post-tag-35-SOH, body_end+7)` collects every SOH offset into a stack `std::uint32_t sohs[kMaxFields]`, then a scalar second pass walks the array and parses tag digits between consecutive known boundaries. Checksum field (`"10=NNN<SOH>"`) is validated directly from the last SOH. Dead helpers (`find_soh_avx2`, `scan_one_field`) removed.

| Metric            | AVX2 (per-field) | AVX2 (bulk SOH) | Δ       |
|-------------------|------------------|-----------------|---------|
| p50 time          | 75.9 µs          | 68.0 µs         | -10%    |
| Per msg           | 63.8 ns          | 57.1 ns         | -10%    |
| Throughput        | 1.906 GiB/s      | 2.19 GiB/s      | +15%    |
| Msg/s             | 13.20 M          | 14.72 M         | +12%    |
| Instructions/msg  | 1,110            | 1,253           | +13%    |
| Cycles/msg        | 285              | 257             | -10%    |
| Branches/msg      | 229              | 271             | +18%    |
| L1-D misses/msg   | 0.26             | 0.35            | +0.09   |
| IPC               | 3.90             | 4.88            | +25%    |

More total instructions (bulk SIMD pass scans body bytes, scalar pass still walks tag bytes) but fewer cycles — the SIMD pass is branchless and runs near-peak, and the scalar pass gets a known end pointer per field so its tag-digit loop drops the `p < end` bounds check against the buffer end in favor of a simple `tp < se` against the next SOH. The per-field path paid AVX2 setup (broadcast, loadu, cmpeq, movemask) once per value; the bulk path amortizes setup across one pass covering all fields, and the inner SOH-extract loop (`countr_zero` + `m &= m - 1`) runs at very high IPC. Instructions budget shifts from branchy scalar to straight-line SIMD.

---

## 2026-04-19 — 64-byte unroll in `find_soh_avx2` (regression, reverted)

Tried two 32-byte loads + cmpeq per loop iter, OR'd into a 64-bit mask, with a 32-byte tail. **Regressed +17%** on this corpus and was reverted.

| Metric            | 32-byte step | 64-byte unroll | Δ          |
|-------------------|--------------|----------------|------------|
| p50 time          | 75.8 µs      | 88.7 µs        | +17%       |
| Per msg           | 63.7 ns      | 74.6 ns        | +17%       |
| Instructions/msg  | 1,110        | 1,099          | -1%        |
| Cycles/msg        | 284          | 334            | +18%       |

Why it lost: typical FIX values in `bulk.stream` are short (most fields fit in the first 32-byte window). The 64-byte unroll forces two loads + cmpeq + movemask before the mask check — wasted work whenever the SOH was in the first half. The unroll only pays when scans regularly run >32 bytes (long free-text fields, FIXML payloads, raw-data blocks). Worth re-trying if a corpus with longer values shows up.
