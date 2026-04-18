# Benchmark results

Historical benchmark runs produced by `scripts/bench.sh`. Committed to git so
performance over time is inspectable from `git log`.

## File naming

```
YYYY-MM-DD-<short-git-sha>-<machine>.json
```

Google Benchmark's native `--benchmark_out_format=json` output. Keep the
files as-produced — downstream tooling (not yet written) may aggregate them
across runs.
