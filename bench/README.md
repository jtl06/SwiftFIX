# Benchmarks

Google Benchmark harness for comparing stock QuickFIX stream handling with
SwiftFIX scanner implementations.

## Running

```
scripts/build.sh
scripts/bench.sh
```

`bench.sh` writes one timestamped JSON file per run under
`bench/results/`, tagged with the short git SHA and hostname so you can
diff across commits or machines later.

## Benchmarks registered

The harness auto-detects what `SWIFTFIX_CORPUS` points at and registers
whichever benchmarks make sense:

| `SWIFTFIX_CORPUS` points at | Registered benchmark(s)                   |
|-----------------------------|-------------------------------------------|
| directory (default: `corpus/valid/`) | `QuickFIX_SetString`             |
| file (e.g. `corpus/bulk.stream`)     | `QuickFIX_StreamSplit`, `QuickFIX_StreamParse`, `SwiftFIX_ScalarSplit`, and `SwiftFIX_Avx2Split` when AVX2 is available |

- **QuickFIX_SetString** — pre-split frames fed into
  `FIX::Message::setString(validate=false)`. Measures tag parsing in
  isolation.
- **QuickFIX_StreamSplit** — raw byte stream into `FIX::Parser`, calling
  only `readFixMessage` (no `setString`). Measures frame-boundary
  detection in isolation — the apples-to-apples counterpart for a SIMD
  scanner that only splits.
- **QuickFIX_StreamParse** — raw byte stream in, full `FIX::Message`
  objects out. This is the stock on-the-wire path and the apples-to-
  apples counterpart for a pre-parser that drives the shim end-to-end.
- **SwiftFIX_ScalarSplit / SwiftFIX_Avx2Split** — raw byte stream scanned
  frame-by-frame, advancing by `FieldIndex::frame_length`. Measures scanner
  throughput without constructing `FIX::Message` objects.

## Machine-independent signals (perf counters)

Wall-clock nanoseconds move with CPU frequency, thermal state, and
neighbor processes. For numbers that travel across machines, use
hardware performance counters: instructions retired, branches, cache
misses, stalls. Those are properties of the code+input, not the box.

### One-time setup

```
sudo apt install libpfm4-dev          # Debian/Ubuntu
sudo sysctl kernel.perf_event_paranoid=1
```

Then re-run `scripts/build.sh`. CMake prints `perf counters: enabled` in
the configuration summary when libpfm is picked up.

`perf_event_paranoid` defaults to `4` on many distros, which hides PMU
counters from unprivileged users. `1` lets any user read their own
process's counters; `2` also works but blocks kernel tracing. Persist
with `/etc/sysctl.d/99-perf.conf` if you want it across reboots.

### Usage

```
SWIFTFIX_CORPUS=corpus/bulk.stream scripts/bench.sh \
    --benchmark_perf_counters=INSTRUCTIONS,BRANCHES,BRANCH-MISSES,CYCLES
```

Google Benchmark reports each counter as a per-iteration total in the
JSON output. Useful derived numbers:

- **Instructions / byte**  = `INSTRUCTIONS` / `bytes_per_second * time`
- **Instructions / message** = `INSTRUCTIONS` / `items_per_second * time`
- **IPC** = `INSTRUCTIONS` / `CYCLES`
- **Branch miss rate** = `BRANCH-MISSES` / `BRANCHES`

These hold steady across machines with the same microarch family, and
the *ratios* hold across arches too — which is what you want when
comparing a scalar scanner on a laptop to a SIMD scanner on a
colocated production box.

### Counter names

libpfm exposes the generic PMU events (`CYCLES`, `INSTRUCTIONS`,
`BRANCHES`, `BRANCH-MISSES`, `CACHE-REFERENCES`, `CACHE-MISSES`,
`STALLED-CYCLES-FRONTEND`, `STALLED-CYCLES-BACKEND`) plus every vendor-
specific event your CPU supports. `showevtinfo` from the `libpfm4-tools`
package lists them.

## Stability notes

- The bench is deterministic on a fixed corpus + seed. If numbers
  wobble, first check `CPU scaling is enabled` in the preamble and pin
  the governor (`sudo cpupower frequency-set -g performance`).
- `bulk.stream` is reproducible from the seed (`corpus/generate.py
  --bulk N --seed S`). Record which seed you used in any run you plan
  to compare against.
