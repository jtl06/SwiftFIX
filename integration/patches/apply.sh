#!/usr/bin/env bash
# apply.sh — idempotently apply every integration/patches/*.patch to the
# QuickFIX submodule. Invoked from CMake (ExternalProject_Add PATCH_COMMAND)
# so re-configuring a dirty build tree does not fail if patches are already
# in place.
#
# Idempotency strategy: for each patch, ask git apply whether it could be
# reversed cleanly. If yes, the patch is already applied — skip it. If no,
# try applying it forward. Any other error is fatal.
set -euo pipefail

# Resolve paths relative to this script so we work from any CWD.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
QUICKFIX_DIR="${REPO_ROOT}/third_party/quickfix"

if [ ! -d "${QUICKFIX_DIR}" ]; then
    echo "apply.sh: quickfix submodule missing at ${QUICKFIX_DIR}" >&2
    exit 1
fi

shopt -s nullglob
patches=( "${SCRIPT_DIR}"/*.patch )
shopt -u nullglob

if [ ${#patches[@]} -eq 0 ]; then
    echo "apply.sh: no patches to apply"
    exit 0
fi

cd "${QUICKFIX_DIR}"

for patch in "${patches[@]}"; do
    name="$(basename "${patch}")"
    if git apply --check --reverse "${patch}" >/dev/null 2>&1; then
        echo "apply.sh: ${name} already applied — skipping"
        continue
    fi
    if ! git apply --check "${patch}" >/dev/null 2>&1; then
        echo "apply.sh: ${name} does not apply cleanly (not already applied, not appliable)" >&2
        git apply --check "${patch}" || true
        exit 1
    fi
    echo "apply.sh: applying ${name}"
    git apply "${patch}"
done
