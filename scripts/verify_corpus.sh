#!/usr/bin/env bash
# verify_corpus.sh — scalar-vs-SIMD equivalence check.
#
# TODO(phase2): replay every corpus file through both the scalar scanner
# and every SIMD scanner compiled into the build, and diff the resulting
# FieldIndex structures byte-for-byte. A divergence is a hard failure.
#
# Intentionally empty in phase 0 — the scalar scanner is still a stub and
# no SIMD scanner has been implemented yet.
set -euo pipefail

echo "verify_corpus.sh is a phase-2 placeholder; nothing to verify yet." >&2
exit 0
