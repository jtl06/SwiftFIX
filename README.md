# SwiftFIX

Faster, SIMD-assisted FIX pre-parser for [QuickFIX C++](https://github.com/quickfix/quickfix).

> Status: Scalar and AVX2 preparsers implemented and benchmarked. AVX-512 / NEON to follow.

SwiftFIX performs structural scanning of inbound FIX tag-value messages by finding `<SOH>` and `=` boundaries, extracting header tag positions, and doing early malformed-frame rejection. It hands QuickFIX a pre-computed field-boundary table.

The SwiftFIX project composes of a hand-written scalar preparser and an AVX2 preparser (WIP: AVX-512 / NEON). SwiftFIX represents a 14× improvement to throughput vs stock QuickFIX `StreamSplit` by replacing its allocation-and-copy frame extraction (a fresh std::string per message, a memmove of the read buffer on every call) with a single linear pass that writes fixed-size offset records into a reused output buffer. This eliminates ~900 L1 cache-line refills per message and lets the CPU pipeline run at ~4 IPC instead of stalling at 0.18. The AVX2 path further drops the value-side SOH scan to a 32-byte `_mm256_cmpeq_epi8`, cutting another -24% cycles vs scalar.

QuickFIX remains the authoritative engine for validation, session state, and message semantics.

## Results

Phase 1 numbers on `corpus/bulk.stream` (1190 messages, Release build, i7-12650H, CPU scaling enabled — treat as rough):

| Benchmark              | p50 time | Per msg  | Throughput  | Msg/s   |
|------------------------|----------|----------|-------------|---------|
| `QuickFIX_StreamSplit` | 1.04 ms  | 874 ns   | 146 MiB/s   | 959 k   |
| `SwiftFIX_ScalarSplit` | 100.2 µs | 84.2 ns  | 1.49 GiB/s  | 10.01 M |
| `SwiftFIX_Avx2Split`   | 75.9 µs  | 63.8 ns  | 1.96 GiB/s  | 13.20 M |

`ScalarSplit` and `Avx2Split` are SwiftFIX equivalents to `StreamSplit`, all producing frame boundaries for `StreamParse`. The AVX2 path uses a 32-byte `_mm256_cmpeq_epi8` SOH scan on the wide value side; tag parsing and the fixed header (tags 8/9/35) stay scalar because their spans are too short for SIMD to pay off. Runtime dispatch via `__builtin_cpu_supports("avx2")` falls back to scalar on non-AVX2 hosts.

### Perf counters (libpfm4)

Per-message (per-iteration counts ÷ 1190). Re-run with `--benchmark_perf_counters=...`.

| Metric            | `StreamSplit` | `ScalarSplit` | `Avx2Split` | Ratio (AVX2 vs QF) |
|-------------------|---------------|---------------|-------------|--------------------|
| Instructions/msg  | 722           | 1,387         | 1,110       | 1.5× more          |
| Branches/msg      | 168           | 374           | 229         | 1.4× more          |
| Cycles/msg        | 3,999         | 376           | 285         | **14× fewer**      |
| IPC               | 0.18          | 3.69          | 3.90        | **22× higher**     |
| L1-D misses/msg   | 911           | 0.04          | 0.26        | **~3,500× fewer**  |

Both SwiftFIX splitters are more computationally intensive than QuickFIX but finish in a fraction of the cycles, as they are compute-bound on hot L1 data. QuickFIX is memory-bound: ~900 L1 refills per message (per-frame `std::string` allocations, `FIX::Parser` buffer state, virtual dispatch) × ~12-cycle L2 latency ≈ the entire cycle gap. LLC is cold for all three — `bulk.stream` (~150 KiB) fits in L2. AVX2 narrows the SwiftFIX work further: -20% instructions and -24% cycles vs scalar, almost entirely from the 32-byte SOH compare replacing the byte-by-byte loop.

## Repository layout

```
preparser/       libswiftfix — the pre-parser (scalar + SIMD (WIP) scanners)
integration/     QuickFIX patches + shim that consumes the FieldIndex
bench/           Google Benchmark harness against corpus/valid/
corpus/          Test messages (valid, malformed, edge cases)
profile/         Phase 0 profiling artifacts (flame graphs, notes)
docs/            Design docs
scripts/         Build, bench, profile convenience scripts
third_party/     QuickFIX pinned as a submodule (v1.15.1)
```

## Build

Requires CMake 3.20+, a C++20 compiler (GCC 11+ or Clang 14+), and Python 3 for the corpus generator.

```bash
git clone --recursive https://example.invalid/swiftfix.git
cd swiftfix
scripts/build.sh             # Release build into build/release/
scripts/build_debug.sh       # Debug + ASan + UBSan into build/debug/
```

If you cloned without `--recursive`:

```bash
git submodule update --init --recursive
```

## Test

```bash
(cd build/release && ctest --output-on-failure)
```

## Bench
```bash
scripts/bench.sh             # writes JSON to bench/results/
```

Baselines per machine are committed under `bench/baseline/`. See its README for naming conventions.

## Regenerating the corpus

`corpus/valid/` is reproducible from `corpus/generate.py`. Edit templates there, then:

```bash
python3 corpus/generate.py
```

See `corpus/README.md` for corpus organization, and `docs/embedded_data.md` for the strategy around tag-95/96 edge cases.

## License

MIT — see `LICENSE`. Vendored QuickFIX retains its own license, see `third_party/quickfix/LICENSE`.
