#!/usr/bin/env bash
# build.sh — clean Release build.
#
# Usage:
#   scripts/build.sh              # Release build into build/release
#   scripts/build.sh --clean      # wipe build dir first
#   scripts/build.sh --clang      # use clang++ (default: system gcc)
#   scripts/build.sh --gcc        # use g++ explicitly
#   scripts/build.sh --jobs 8     # parallelism (default: nproc)
#
# You can also override the compiler via env vars: CC=clang CXX=clang++.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build/release"

clean=0
compiler=""
jobs="$(nproc 2>/dev/null || echo 4)"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --clean)   clean=1; shift ;;
        --clang)   compiler="clang"; shift ;;
        --gcc)     compiler="gcc"; shift ;;
        --jobs)    jobs="$2"; shift 2 ;;
        -h|--help)
            sed -n '2,12p' "$0"; exit 0 ;;
        *) echo "unknown flag: $1" >&2; exit 2 ;;
    esac
done

case "${compiler}" in
    clang) export CC=clang   CXX=clang++ ;;
    gcc)   export CC=gcc     CXX=g++     ;;
    "")    : ;;  # honor whatever CC/CXX the environment already has
esac

if [[ ! -f "${REPO_ROOT}/third_party/quickfix/CMakeLists.txt" ]]; then
    echo "QuickFIX submodule missing. Run:" >&2
    echo "  git submodule update --init --recursive" >&2
    exit 1
fi

if [[ "${clean}" -eq 1 ]]; then
    rm -rf "${BUILD_DIR}"
fi

cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DSWIFTFIX_NATIVE=ON

cmake --build "${BUILD_DIR}" --parallel "${jobs}"

echo
echo "Build succeeded. Run tests with:"
echo "  (cd ${BUILD_DIR} && ctest --output-on-failure)"
