#!/usr/bin/env bash
# verify_corpus.sh — scalar-vs-SIMD equivalence check.
#
# Builds the release tree if needed, then runs the corpus scanner tests.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build/release"

if [[ ! -x "${BUILD_DIR}/preparser/tests/swiftfix_unit_tests" ]]; then
    "${REPO_ROOT}/scripts/build.sh"
fi

cd "${BUILD_DIR}"
ctest --output-on-failure -R ScannerCorpus
