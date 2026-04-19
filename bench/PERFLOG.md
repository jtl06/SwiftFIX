# Perf log

Per-change numbers for `SwiftFIX_ScalarSplit` on `corpus/bulk.stream` (1190 msgs, i7-12650H P-core pin, Release, CPU scaling on). Run via `make bench-perf`. Per-message counters = per-iter ÷ 1190.

---

## Reference — QuickFIX (commit `42ce312`)

Stock `FIX::Parser` on the same corpus. Here as the comparison baseline.

| Metric            | `StreamSplit` | `StreamParse` |
|-------------------|---------------|---------------|
| p50 time          | 1.04 ms       | 1.91 ms       |
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
| Throughput        | 1.227 GiB/s | 1.275 GiB/s | +3.9% |
| Msg/s             | 8.26 M  | 8.59 M  | +4.0%      |
| Instructions/msg  | 1,761   | 1,706   | -3.1%      |
| Cycles/msg        | 458     | 439     | -4.1%      |
| Branches/msg      | 453     | 428     | -5.5%      |
| L1-D misses/msg   | 0.21    | 0.33    | +0.12      |
| IPC               | 3.85    | 3.89    | +1%        |
