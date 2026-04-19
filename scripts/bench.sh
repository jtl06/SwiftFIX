#!/usr/bin/env bash
# bench.sh — run the benchmark harness and save results to bench/results/.
#
# Runs each registered benchmark in its own process so allocator state and
# cache contents from one bench don't bleed into the next. Pins every run
# to a single P-core on hybrid CPUs (Alder Lake / Raptor Lake) so samples
# don't migrate between P and E cores mid-run.
#
# Assumes scripts/build.sh has been run. Rebuilds if the bench binary is
# missing. Extra flags after the script name are forwarded verbatim to
# the bench binary (e.g. --benchmark_perf_counters=INSTRUCTIONS,CYCLES).
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build/release"
BENCH_BIN="${BUILD_DIR}/bench/swiftfix_bench"
RESULTS_DIR="${REPO_ROOT}/bench/results"

# Corpus selection: caller can override via SWIFTFIX_CORPUS env var.
# Point it at corpus/bulk.stream for the stream benches.
CORPUS_DIR="${SWIFTFIX_CORPUS:-${REPO_ROOT}/corpus/valid}"

if [[ ! -x "${BENCH_BIN}" ]]; then
    echo "Bench binary missing; running scripts/build.sh first."
    "${REPO_ROOT}/scripts/build.sh"
fi

# Pick a P-core CPU to pin to. On hybrid Intel (Alder Lake / Raptor Lake)
# the kernel exposes the P-core set as /sys/devices/cpu_core/cpus. Falls
# back to CPU 0 on non-hybrid systems.
pin_cpu=0
if [[ -r /sys/devices/cpu_core/cpus ]]; then
    pcores="$(cat /sys/devices/cpu_core/cpus)"
    pin_cpu="${pcores%%[-,]*}"
    echo "Hybrid CPU detected (P-cores: ${pcores}); pinning to CPU ${pin_cpu}."
else
    echo "Non-hybrid CPU; pinning to CPU ${pin_cpu}."
fi

if ! command -v taskset >/dev/null 2>&1; then
    echo "taskset not found; install util-linux or unset pinning." >&2
    exit 1
fi

mkdir -p "${RESULTS_DIR}"
stamp="$(date -u +%Y-%m-%dT%H-%M-%SZ)"
sha="$(git -C "${REPO_ROOT}" rev-parse --short HEAD 2>/dev/null || echo nogit)"
host="$(hostname -s 2>/dev/null || echo unknown)"

# Enumerate registered benchmarks. Uses the same corpus env as the real
# run so auto-registration in harness.cpp picks the same set.
benches="$(SWIFTFIX_CORPUS="${CORPUS_DIR}" "${BENCH_BIN}" --benchmark_list_tests=true)"
if [[ -z "${benches}" ]]; then
    echo "No benchmarks registered — check SWIFTFIX_CORPUS=${CORPUS_DIR}" >&2
    exit 1
fi

echo "Corpus: ${CORPUS_DIR}"
echo "Output: ${RESULTS_DIR}/${stamp}-${sha}-${host}-<bench>.json"
echo

for name in ${benches}; do
    # Benchmark names can contain '/' and ':' (e.g. "Foo/repeats:10"); sanitize
    # them out of the filename but keep the filter regex pointed at the real name.
    safe_name="${name//\//_}"
    safe_name="${safe_name//:/_}"
    out="${RESULTS_DIR}/${stamp}-${sha}-${host}-${safe_name}.json"
    echo ">>> ${name}"
    SWIFTFIX_CORPUS="${CORPUS_DIR}" \
        taskset -c "${pin_cpu}" \
        "${BENCH_BIN}" \
            --benchmark_filter="^${name}\$" \
            --benchmark_out_format=json \
            --benchmark_out="${out}" \
            --benchmark_counters_tabular=true \
            "$@"
    echo
done

echo "Wrote JSON per benchmark to ${RESULTS_DIR}/"
