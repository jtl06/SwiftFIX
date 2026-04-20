# QuickFIX patches

Minimal patch series applied to the pinned QuickFIX submodule
(`third_party/quickfix`, tag `v1.15.1`) so it can consume a precomputed
`swiftfix::FieldIndex` instead of rescanning a message from byte zero.

## Current patches

```
0001-expose-parse-from-field-index.patch
```

## Rules for this patch series

1. **Keep each patch surgical.** One purpose per patch, small diff. Easier
   to rebase onto future QuickFIX releases; easier to isolate regressions.
2. **No behavior change without `SWIFTFIX_ENABLE_PREPARSE` feature flag.**
   The patched binary must pass QuickFIX's own test suite unmodified when
   the flag is off — this is enforceable in CI.
3. **Never modify QuickFIX's public header ABI without a corresponding bump
   in `third_party/quickfix/README.md` pin notes.** Downstream consumers of
   vendored QuickFIX may rely on binary compatibility.
4. **Each patch carries a `Rationale:` block** in its commit message citing
   the profiling evidence (a flame graph, a cycle-count delta) that
   motivated it. Commits without a rationale block do not merge.

## Applying / rebasing

Patches are applied automatically by CMake through `apply.sh`. To apply or
check them manually from the SwiftFIX repo root:

```
cd third_party/quickfix
git apply ../../integration/patches/*.patch
```

When bumping the QuickFIX pin:

```
cd third_party/quickfix
git checkout <new-tag>
git apply --check ../../integration/patches/*.patch  # dry-run first
```

Conflicts are resolved by regenerating the patches against the new base tag
and re-running the full bench + CI suite.
