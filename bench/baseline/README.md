# Baselines

Stock-QuickFIX numbers for a given machine, used as the reference every
SwiftFIX change is measured against.

Files here are **committed to the repo** so regressions are visible in PR
diffs.

## File naming

```
<machine-class>-<cpu>-<kernel>-<build-type>.txt
```

Examples:

```
bench-host-zen4-linux6.1-Release.txt
dev-x86-64-icelake-linux6.5-Release.txt
```

## What goes in a baseline file

1. Header block with:
   - QuickFIX tag pin (`v1.15.1`)
   - Compiler + version
   - CMake flags (`-DSWIFTFIX_NATIVE=ON` or not, etc.)
   - CPU model, frequency, governor setting
   - Kernel version, turbo/perf-governor state
2. Raw Google Benchmark JSON output from `scripts/bench.sh`.

## Regenerating

```
scripts/bench.sh
```

Copy the relevant JSON from `bench/results/` into a named baseline file only
when you want to pin a machine-specific reference point.
