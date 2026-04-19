# SwiftFIX

Faster (Work in progress: SIMD-assisted)  FIX pre-parser for [QuickFIX C++](https://github.com/quickfix/quickfix).

> Status: Scalar preparser implemented and benchmarked. Optimization and SIMD preparser to follow.

SwiftFIX performs structural scanning of inbound FIX tag-value messages — finding `<SOH>` and `=` boundaries, extracting header tag positions, and doing early malformed-frame rejection. It hands QuickFIX a pre-computed field-boundary table.

The SwiftFIX project composes of a hand written scalar preparser (WIP: and a SIMD preparser for AVX2/AVX512/NEON machines).SwiftFIX represents an 8× improvement to throughput by replacing QuickFIX's allocation-and-copy frame extraction (a fresh std::string per message, a memmove of the read buffer on every call) with a single linear pass that writes fixed-size offset records into a reused output buffer. This eliminates ~900 L1 cache-line refills per message and letting the CPU pipeline run at ~3.8 IPC instead of stalling at 0.18.

QuickFIX remains the authoritative engine for validation, session state, and message semantics.

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

## Benchmark

```bash
scripts/bench.sh             # writes JSON to bench/results/
```

Baselines per machine are committed under `bench/baseline/`. See its README for naming conventions.

Phase 1 numbers on `corpus/bulk.stream` (1190 messages, Release build, i7-12650H, CPU scaling enabled — treat as rough):

| Benchmark              | p50 time | Throughput  | Msg/s  |
|------------------------|----------|-------------|--------|
| `QuickFIX_StreamSplit` | 1.04 ms  | 146 MiB/s   | 959 k  |
| `SwiftFIX_ScalarSplit` | 123 µs   | 1.21 GiB/s  | 8.12 M |
|                        |          |             |        |
| `QuickFIX_StreamParse` | 1.91 ms  | 80 MiB/s    | 525 k  |

`ScalarSplit` is the Swiftfix equivalent to `StreamSplit`, both producing frame boundarie for `StreamParse`. 

### Perf counters (libpfm4)

Per-message (per-iteration counts ÷ 1190). Re-run with `--benchmark_perf_counters=...`.

| Metric            | `StreamSplit` | `ScalarSplit` | Ratio           |
|-------------------|---------------|---------------|-----------------|
| Instructions/msg  | 722           | 1,788         | 2.5× more       |
| Branches/msg      | 168           | 453           | 2.7× more       |
| Cycles/msg        | 3,999         | 473           | **8.5× fewer**  |
| IPC               | 0.18          | 3.78          | **21× higher**  |
| L1-D misses/msg   | 911           | 0.76          | **~1200× fewer**|

The SwiftFIX splitter is more computationally intensive, but finishes finishes in one-eighth the cycles, as it is compute-bound on hot L1 data. QuickFIX is memory-bound: ~900 L1 refills per message (per-frame `std::string` allocations, `FIX::Parser` buffer state, virtual dispatch) × ~12-cycle L2 latency ≈ the entire cycle gap. LLC is cold for both — `bulk.stream` (~150 KiB) fits in L2.

## Profile

```bash
scripts/profile.sh           # perf record → flame graph SVG
```

The script checks for `perf` and the FlameGraph scripts and prints install instructions if either is missing. Output lands in `profile/flamegraphs/flamegraph-<timestamp>.svg`.

## Regenerating the corpus

`corpus/valid/` is reproducible from `corpus/generate.py`. Edit templates there, then:

```bash
python3 corpus/generate.py
```

See `corpus/README.md` for corpus organization, and `docs/embedded_data.md` for the strategy around tag-95/96 edge cases.

## License

MIT — see `LICENSE`. Vendored QuickFIX retains its own license, see `third_party/quickfix/LICENSE`.
