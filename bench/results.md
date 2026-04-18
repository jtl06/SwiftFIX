# Phase-0 baseline — stock QuickFIX

Baseline throughput for `FIX::Parser` + `FIX::Message::setString` on a
synthetic 1000-message stream. This is the bar every future SIMD
pre-parser change is measured against. No pre-parser in this path — the
numbers below are what QuickFIX does today, unmodified.

**Last measured:** 2026-04-18 · commit `0a690a0`

## Environment

| | |
|-|-|
| CPU | 12th Gen Intel Core i7-12650H, 10 cores / 16 threads, 4.6 GHz boost |
| Kernel | Linux 6.14.0-37-generic x86_64 |
| Compiler | Clang 18.1.3, C++20, `-O3 -march=native` |
| QuickFIX | v1.15.1 vendored, shared-lib, `validate=false` throughout |
| Corpus | `corpus/bulk.stream` — 1000 messages, 159,469 bytes (seed 42) |
| CPU scaling | enabled (kernel didn't allow fixing the governor in this run) |

## Results

Ten repetitions, 0.2s min per repetition, medians reported.

| Benchmark | Throughput (bytes) | Throughput (msgs) | ns / msg | Iterations/rep |
|---|---:|---:|---:|---:|
| `QuickFIX_StreamSplit` (framing only) | **143.9 MiB/s** | **946 k/s** | 1057.6 | 240 |
| `QuickFIX_StreamParse` (framing + tag parse) | **79.3 MiB/s** | **521 k/s** | 1920.6 | 146 |

`StreamSplit` calls `FIX::Parser::readFixMessage` to extract each frame
but never hands it to `setString`. The delta between the two rows is
therefore the tag-parsing cost alone: ~60% of total wall time goes into
tag parsing, ~40% into framing.

## Hardware counters (machine-independent)

Wall-clock numbers above move with CPU frequency, thermals, and
neighbor processes. The numbers below don't — they're properties of the
compiled code + input, not the box it ran on. Use these for
cross-machine comparisons.

Medians per iteration. An iteration = 1000 messages / 159,469 bytes.

| Counter | `StreamSplit` | `StreamParse` |
|---|---:|---:|
| INSTRUCTIONS | 859.76 k | 14.35 M |
| BRANCHES | 200.01 k | 3.17 M |
| BRANCH-MISSES | 411 | 11.76 k |
| CACHE-REFERENCES | 39 | 36 |
| CACHE-MISSES | 15 | 15 |
| CYCLES | 4.74 M | 8.72 M |

### Derived per-message

| Metric | `StreamSplit` | `StreamParse` |
|---|---:|---:|
| Instructions / msg | 860 | 14,352 |
| Instructions / byte | 5.39 | 90.0 |
| Branches / msg | 200 | 3,175 |
| Branch-miss rate | 0.21 % | 0.37 % |
| Cycles / msg | 4,738 | 8,720 |
| **IPC** | **0.18** | **1.65** |

### What these numbers say

- **IPC of 0.18 on `StreamSplit` is the anomaly.** 860 instructions
  should retire in ~500–800 cycles on Golden Cove, not 4,700. The
  cache-miss rate is ~0.03% of references, so it isn't memory-bound.
  Most likely suspect: the `std::string` that `readFixMessage` returns
  forces an allocator round-trip per message; that path serializes
  through libc and stalls the front end. A SIMD pre-parser that emits
  into a caller-provided arena would avoid this entirely.
- **`StreamParse` IPC of 1.65 is healthy** — the ~14 k instructions and
  3 k branches per message are doing the real work (per-tag loop, token
  conversion, map inserts into `FieldMap`). Branch misprediction at
  0.37% is decent given FIX's tag-dispatch patterns.
- **Framing is 17× cheaper in instruction count than full parsing**
  (860 vs 14,352). A pre-parser that only does framing leaves most of
  the wall time on the table — to move the top-line number, the
  pre-parser also has to produce a field index that `setString`
  consumers can bypass.

## Reproducing

```
python3 corpus/generate.py --bulk 1000 --seed 42
scripts/build.sh --clang
SWIFTFIX_CORPUS=corpus/bulk.stream scripts/bench.sh \
    --benchmark_perf_counters=INSTRUCTIONS,BRANCHES,BRANCH-MISSES,CACHE-REFERENCES,CACHE-MISSES,CYCLES \
    --benchmark_repetitions=10 \
    --benchmark_min_time=0.2s
```

The raw JSON for each run lands under `bench/results/` tagged with
timestamp + short SHA + hostname. This file is the human-readable
summary; diff against the JSON for anything quantitative.

## Known caveats for this run

- CPU governor was not pinned; `***WARNING*** CPU scaling is enabled`
  printed in the preamble. ns-numbers carry ~2–3% CV as a result. The
  PMU counters are unaffected — they count events, not time.
- GCC 13 on this host rejects QuickFIX's dynamic-exception-spec
  headers under `-std=c++20 -Wpedantic`; Clang accepts them as a
  warning. Until that's addressed, `scripts/build.sh --clang` is the
  supported path.
- `validate=false` everywhere. With validation on, QuickFIX does data-
  dictionary lookups per tag, which roughly doubles the instruction
  count. Our pre-parser targets the validate-off path (validation is a
  QuickFIX-layer concern, not a framing concern).
