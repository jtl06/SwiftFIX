# Phase 0 profiling notes

TODO — to be filled in after `scripts/profile.sh` runs.

Structure each profiling session here as:

```
## YYYY-MM-DD on <machine> (<cpu>, <compiler>)

- Corpus slice: corpus/valid (size)
- Build flags: -DCMAKE_BUILD_TYPE=Release -DSWIFTFIX_NATIVE=ON
- Invocation: <command used>
- Flame graph: flamegraphs/flamegraph-<timestamp>.svg

### Findings

- What fraction of CPU is in QuickFIX::Message::setString / Parser::extractField?
- Which substring scans dominate?
- Is the scan path actually worth SIMD-ing? (Honest answer, even if "no".)

### Next steps

- Concrete actions derived from the findings.
```

Findings here gate the phase 1 decision to write a scalar scanner at all.
