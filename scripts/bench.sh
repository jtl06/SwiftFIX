#!/usr/bin/env bash
# bench.sh — run the benchmark harness against corpus/valid/ and save
# results to bench/results/.
#
# Assumes scripts/build.sh has been run. Rebuilds if the bench binary is
# missing. Results are timestamped and include the short git SHA when
# available.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build/release"
BENCH_BIN="${BUILD_DIR}/bench/swiftfix_bench"
RESULTS_DIR="${REPO_ROOT}/bench/results"

# Corpus selection: caller can override via SWIFTFIX_CORPUS env var.
# Point it at corpus/bulk.stream for the stream-parse benchmark.
# Defaults to the canonical per-file set.
CORPUS_DIR="${SWIFTFIX_CORPUS:-${REPO_ROOT}/corpus/valid}"

if [[ ! -x "${BENCH_BIN}" ]]; then
    echo "Bench binary missing; running scripts/build.sh first."
    "${REPO_ROOT}/scripts/build.sh"
fi

mkdir -p "${RESULTS_DIR}"

stamp="$(date -u +%Y-%m-%dT%H-%M-%SZ)"
sha="$(git -C "${REPO_ROOT}" rev-parse --short HEAD 2>/dev/null || echo nogit)"
host="$(hostname -s 2>/dev/null || echo unknown)"
out="${RESULTS_DIR}/${stamp}-${sha}-${host}.json"

echo "Corpus:  ${CORPUS_DIR}"
echo "Output:  ${out}"
echo

SWIFTFIX_CORPUS="${CORPUS_DIR}" \
    "${BENCH_BIN}" \
        --benchmark_out_format=json \
        --benchmark_out="${out}" \
        --benchmark_counters_tabular=true \
        "$@"

echo
echo "Wrote: ${out}"
