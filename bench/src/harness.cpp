#include "harness.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include <benchmark/benchmark.h>
#include <quickfix/Message.h>

#include "replay.hpp"

namespace swiftfix::bench {

namespace {

// Resolve corpus dir: env var SWIFTFIX_CORPUS wins, otherwise the baked-in
// compile-time constant (a default set by CMake).
std::filesystem::path resolve_corpus_dir() {
    if (const char* env = std::getenv("SWIFTFIX_CORPUS")) {
        return env;
    }
#ifdef SWIFTFIX_CORPUS_DIR
    return SWIFTFIX_CORPUS_DIR;
#else
    return "corpus/valid";
#endif
}

// Load once per process. The benchmark framework calls benchmarks many
// times; reloading per iteration would dominate the measurement.
const std::vector<CorpusMessage>& corpus() {
    static const auto cached = [] {
        return load_corpus(resolve_corpus_dir());
    }();
    return cached;
}

// Parse every message in the corpus once through stock QuickFIX. The state
// arg is the same corpus in a loop; timing starts and stops around the
// parsing only.
void BM_QuickFIX_SetString(benchmark::State& state) {
    const auto& msgs = corpus();
    if (msgs.empty()) {
        state.SkipWithError("corpus is empty");
        return;
    }

    std::size_t bytes = 0;
    for (const auto& m : msgs) bytes += m.bytes.size();

    for (auto _ : state) {
        for (const auto& m : msgs) {
            FIX::Message parsed;
            try {
                parsed.setString(m.bytes, /*validate=*/false);
                benchmark::DoNotOptimize(parsed);
            } catch (const std::exception&) {
                state.SkipWithError("parse threw");
                return;
            }
        }
    }

    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                            static_cast<std::int64_t>(msgs.size()));
    state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) *
                            static_cast<std::int64_t>(bytes));
}

}  // namespace

void register_benchmarks() {
    benchmark::RegisterBenchmark("QuickFIX_SetString", BM_QuickFIX_SetString)
        ->Unit(benchmark::kNanosecond)
        ->ComputeStatistics("p50", [](const std::vector<double>& v) {
            auto sorted = v;
            std::nth_element(sorted.begin(),
                             sorted.begin() + sorted.size() / 2,
                             sorted.end());
            return sorted[sorted.size() / 2];
        })
        ->ComputeStatistics("p99", [](const std::vector<double>& v) {
            auto sorted = v;
            std::size_t idx = (sorted.size() * 99) / 100;
            if (idx >= sorted.size()) idx = sorted.size() - 1;
            std::nth_element(sorted.begin(),
                             sorted.begin() + static_cast<std::ptrdiff_t>(idx),
                             sorted.end());
            return sorted[idx];
        })
        ->Repetitions(10)
        ->ReportAggregatesOnly(false)
        ->DisplayAggregatesOnly(false);
}

}  // namespace swiftfix::bench
