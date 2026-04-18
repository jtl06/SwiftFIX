#!/usr/bin/env bash
# build_debug.sh — Debug build with ASan + UBSan.
#
# Usage:
#   scripts/build_debug.sh [--clean] [--clang|--gcc] [--jobs N]
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build/debug"

clean=0
compiler=""
jobs="$(nproc 2>/dev/null || echo 4)"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --clean) clean=1; shift ;;
        --clang) compiler="clang"; shift ;;
        --gcc)   compiler="gcc"; shift ;;
        --jobs)  jobs="$2"; shift 2 ;;
        -h|--help)
            sed -n '2,6p' "$0"; exit 0 ;;
        *) echo "unknown flag: $1" >&2; exit 2 ;;
    esac
done

case "${compiler}" in
    clang) export CC=clang   CXX=clang++ ;;
    gcc)   export CC=gcc     CXX=g++     ;;
    "")    : ;;
esac

if [[ "${clean}" -eq 1 ]]; then
    rm -rf "${BUILD_DIR}"
fi

cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DSWIFTFIX_ENABLE_SANITIZERS=ON

cmake --build "${BUILD_DIR}" --parallel "${jobs}"

echo
echo "Debug build succeeded. Run tests with:"
echo "  (cd ${BUILD_DIR} && ctest --output-on-failure)"
