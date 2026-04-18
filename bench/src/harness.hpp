// harness.hpp — benchmark entry points.
//
// The phase-0 harness pushes corpus bytes through FIX::Message::setString
// with validation off, which approximates the hot path QuickFIX takes on
// inbound session messages. Future benchmarks (phase 3+) will add
// variants that route through the shim with preparse enabled.
#pragma once

namespace swiftfix::bench {

// Register all benchmarks with Google Benchmark. Called from main().
void register_benchmarks();

}  // namespace swiftfix::bench
