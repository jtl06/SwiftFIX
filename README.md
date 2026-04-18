# SwiftFIX

SIMD-assisted FIX pre-parser for [QuickFIX C++](https://github.com/quickfix/quickfix).

> **Status: phase 0 scaffold.** Library API is declared; no optimization
> work has landed yet. The first job is profiling stock QuickFIX on the
> corpus to confirm the scan path is actually hot. See `docs/design.md`.

SwiftFIX performs structural scanning of inbound FIX tag-value messages —
finding `<SOH>` and `=` boundaries, extracting header tag positions, and
doing early malformed-frame rejection — and hands QuickFIX a pre-computed
field-boundary table so it doesn't rescan the message from byte zero.

QuickFIX remains the authoritative engine for validation, session
management, and message semantics. The pre-parser is *advisory* and sits
behind a feature flag; any anomaly falls back to stock QuickFIX.

## Repository layout

```
preparser/       libswiftfix — the pre-parser (scalar + SIMD scanners)
integration/     QuickFIX patches + shim that consumes the FieldIndex
bench/           Google Benchmark harness against corpus/valid/
corpus/          Test messages (valid, malformed, edge cases)
profile/         Phase 0 profiling artifacts (flame graphs, notes)
docs/            Design docs
scripts/         Build, bench, profile convenience scripts
third_party/     QuickFIX pinned as a submodule (v1.15.1)
```

## Build

Requires CMake 3.20+, a C++20 compiler (GCC 11+ or Clang 14+), and
Python 3 for the corpus generator.

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

Baselines per machine are committed under `bench/baseline/`. See its
README for naming conventions.

## Profile

```bash
scripts/profile.sh           # perf record → flame graph SVG
```

The script checks for `perf` and the FlameGraph scripts and prints install
instructions if either is missing. Output lands in
`profile/flamegraphs/flamegraph-<timestamp>.svg`.

## Regenerating the corpus

`corpus/valid/` is reproducible from `corpus/generate.py`. Edit templates
there, then:

```bash
python3 corpus/generate.py
```

See `corpus/README.md` for corpus organization, and
`docs/embedded_data.md` for the strategy around tag-95/96 edge cases.

## License

MIT — see `LICENSE`. Vendored QuickFIX retains its own license, see
`third_party/quickfix/LICENSE`.
