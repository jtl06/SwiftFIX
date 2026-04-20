// harness.hpp — benchmark entry points.
//
// Registers QuickFIX baselines and SwiftFIX scanner benchmarks. Corpus mode
// determines whether benchmarks run over pre-split files or a raw stream.
#pragma once

namespace swiftfix::bench {

// Register all benchmarks with Google Benchmark. Called from main().
void register_benchmarks();

}  // namespace swiftfix::bench
