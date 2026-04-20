#!/usr/bin/env bash
# profile.sh — run the bench harness under perf and produce a flame graph.
#
# Requires:
#   - perf         (linux-tools-generic on Ubuntu)
#   - flamegraph.pl + stackcollapse-perf.pl from https://github.com/brendangregg/FlameGraph
#     Set FLAMEGRAPH_DIR to point at a checkout, or put the scripts in PATH.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build/release"
BENCH_BIN="${BUILD_DIR}/bench/swiftfix_bench"
CORPUS_DIR="${REPO_ROOT}/corpus/valid"
OUT_DIR="${REPO_ROOT}/build/profile/flamegraphs"

missing=0

if ! command -v perf >/dev/null 2>&1; then
    cat <<EOF >&2
perf not found. Install with one of:
  Ubuntu / Debian:   sudo apt install linux-tools-generic linux-tools-\$(uname -r)
  Fedora / RHEL:     sudo dnf install perf
Then confirm 'perf --version' works.
EOF
    missing=1
fi

find_flamegraph() {
    local name="$1"
    if [[ -n "${FLAMEGRAPH_DIR:-}" && -x "${FLAMEGRAPH_DIR}/${name}" ]]; then
        echo "${FLAMEGRAPH_DIR}/${name}"; return 0
    fi
    if command -v "${name}" >/dev/null 2>&1; then
        command -v "${name}"; return 0
    fi
    return 1
}

if ! stackcollapse="$(find_flamegraph stackcollapse-perf.pl)" \
   || ! flamegraph="$(find_flamegraph flamegraph.pl)"; then
    cat <<EOF >&2
FlameGraph scripts not found. Install:
  git clone https://github.com/brendangregg/FlameGraph /opt/FlameGraph
  export FLAMEGRAPH_DIR=/opt/FlameGraph
Then re-run this script.
EOF
    missing=1
fi

if [[ "${missing}" -eq 1 ]]; then exit 1; fi

if [[ ! -x "${BENCH_BIN}" ]]; then
    echo "Bench binary missing; running scripts/build.sh first."
    "${REPO_ROOT}/scripts/build.sh"
fi

mkdir -p "${OUT_DIR}"
stamp="$(date -u +%Y-%m-%dT%H-%M-%SZ)"
data_file="${OUT_DIR}/perf-${stamp}.data"
stacks_file="${OUT_DIR}/perf-${stamp}.stacks"
folded_file="${OUT_DIR}/perf-${stamp}.folded"
svg_file="${OUT_DIR}/flamegraph-${stamp}.svg"

echo "Recording perf data (use Ctrl-C once the run is long enough)..."
SWIFTFIX_CORPUS="${CORPUS_DIR}" \
    perf record -F 999 -g --call-graph=dwarf \
        -o "${data_file}" \
        -- "${BENCH_BIN}" --benchmark_min_time=5s

perf script -i "${data_file}" > "${stacks_file}"
"${stackcollapse}" "${stacks_file}" > "${folded_file}"
"${flamegraph}" "${folded_file}" > "${svg_file}"

echo
echo "Flame graph: ${svg_file}"
echo "Raw data:    ${data_file}"
