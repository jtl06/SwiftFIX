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
