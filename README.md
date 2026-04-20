# SwiftFIX

SIMD-assisted FIX pre-parser for [QuickFIX C++](https://github.com/quickfix/quickfix), built to reduce the cost of splitting inbound FIX tag-value streams before handing messages to QuickFIX for validation, session state, and semantics.

SwiftFIX scans raw FIX bytes for `<SOH>` and `=` boundaries, validates the fixed header, and writes compact field offsets into a reused output buffer. The project includes:

- A scalar scanner with early malformed-frame rejection.
- An AVX2 scanner with runtime CPU dispatch and scalar fallback.
- A small QuickFIX patch/shim path that lets QuickFIX consume the precomputed field index instead of rescanning from byte zero.
- A reproducible benchmark corpus and Google Benchmark harness.

## Start Here

The important code is in `preparser/`.

For a quick technical review, read these files in order:

1. `preparser/include/swiftfix/field_index.hpp` defines the offset table that replaces repeated string extraction.
2. `preparser/src/scanner_scalar.cpp` shows the reference parser and the validation/fallback contract.
3. `preparser/src/scanner_avx2.cpp` shows the SIMD path: one bulk `<SOH>` scan, then scalar tag parsing between known boundaries.
4. `preparser/src/dispatch.cpp` shows runtime AVX2 selection with scalar fallback.

The other folders support that core: `bench/` measures it, `corpus/` feeds it reproducible FIX data, and `integration/` connects it to QuickFIX.

## Why It Is Faster

QuickFIX's stock stream splitter repeatedly extracts frames through allocation-and-copy work: one `std::string` per message and a buffer `memmove` on each dequeue. SwiftFIX replaces that with a single linear pass over the stream and fixed-size offset records written into caller-owned storage.

The AVX2 scanner uses 32-byte `_mm256_cmpeq_epi8` comparisons to collect `<SOH>` offsets across the message body, then parses tag digits between known boundaries. Short fixed-header fields stay scalar because SIMD setup cost is not worth it there.

## Results

Release build on an i7-12650H, using `corpus/bulk.stream` with 1,190 synthetic FIX messages. CPU frequency scaling was enabled, so wall-clock numbers should be treated as local measurements; the relative counters are the more portable signal.

| Benchmark              | p50 time | Per msg | Throughput | Msg/s   |
|------------------------|----------|---------|------------|---------|
| `QuickFIX_StreamSplit` | 1.04 ms  | 874 ns  | 146 MiB/s  | 959 k   |
| `SwiftFIX_ScalarSplit` | 93.2 us  | 78.6 ns | 1.60 GiB/s | 10.75 M |
| `SwiftFIX_Avx2Split`   | 68.0 us  | 57.1 ns | 2.19 GiB/s | 14.72 M |

On this corpus, AVX2 splitting is about **15x faster** than QuickFIX's stock splitter and uses about **27% fewer cycles** than the scalar SwiftFIX scanner.

### Hardware Counters

Per-message counters from libpfm4, computed as per-iteration counts divided by 1,190 messages:

| Metric           | `StreamSplit` | `ScalarSplit` | `Avx2Split` | AVX2 vs QuickFIX |
|------------------|---------------|---------------|-------------|------------------|
| Instructions/msg | 722           | 1,387         | 1,253       | 1.7x more        |
| Branches/msg     | 168           | 374           | 271         | 1.6x more        |
| Cycles/msg       | 3,999         | 353           | 257         | **16x fewer**    |
| IPC              | 0.18          | 3.93          | 4.88        | **27x higher**   |
| L1-D misses/msg  | 911           | 0.05          | 0.35        | **~2,600x fewer** |

SwiftFIX does more explicit parsing work per message, but it keeps the CPU fed from hot cache. The QuickFIX splitter is dominated by memory traffic: per-frame allocation, parser buffer movement, and virtual dispatch show up as roughly 900 L1 data misses per message.

### Larger Corpus

`corpus/bulk25k.stream` contains 25,000 messages and about 5.77 MB of data, with longer News (`35=B`) text and deeper Market Data Incremental (`35=X`) bursts.

| Benchmark              | Per msg  | Throughput |
|------------------------|----------|------------|
| `QuickFIX_StreamSplit` | 91.0 us  | 2.42 MiB/s |
| `SwiftFIX_ScalarSplit` | 135.6 ns | 1.59 GiB/s |
| `SwiftFIX_Avx2Split`   | 93.4 ns  | 2.31 GiB/s |

The larger corpus is useful for stressing long values and cache behavior. It is not the cleanest QuickFIX comparison because `FIX::Parser::readFixMessage` repeatedly memmoves the remaining stream buffer, which scales poorly with multi-megabyte concatenated inputs.

## Build And Test

Requirements:

- CMake 3.20+
- GCC 11+ or Clang 14+
- Python 3 for corpus generation
- Initialized QuickFIX submodule

```bash
git submodule update --init --recursive
scripts/build.sh
(cd build/release && ctest --output-on-failure)
```

Debug builds enable ASan and UBSan:

```bash
scripts/build_debug.sh
```

## Benchmark

```bash
scripts/bench.sh
```

To benchmark a specific stream corpus:

```bash
SWIFTFIX_CORPUS=corpus/bulk.stream scripts/bench.sh
```

To collect hardware counters:

```bash
SWIFTFIX_CORPUS=corpus/bulk.stream scripts/bench.sh \
    --benchmark_perf_counters=INSTRUCTIONS,BRANCHES,BRANCH-MISSES,CYCLES
```

## Project Map

- `preparser/`: scalar and AVX2 scanners plus field-index data structures.
- `integration/`: QuickFIX patch, shim, and end-to-end integration test.
- `bench/`: benchmark harness and recorded benchmark outputs.
- `corpus/`: synthetic valid, malformed, and bulk FIX inputs.

## License

MIT. Vendored QuickFIX retains its upstream license.
