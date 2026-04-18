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
#include <quickfix/Parser.h>

#include "replay.hpp"

namespace swiftfix::bench {

namespace {

// Resolve corpus path: env var SWIFTFIX_CORPUS wins, otherwise the baked-in
// compile-time constant (a default set by CMake).
std::filesystem::path resolve_corpus_path() {
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
const std::vector<CorpusMessage>& split_corpus() {
    static const auto cached = [] {
        return load_corpus(resolve_corpus_path());
    }();
    return cached;
}

const std::string& stream_corpus() {
    static const std::string cached = load_stream(resolve_corpus_path());
    return cached;
}

// Parse every message in the (pre-split) corpus once through stock
// QuickFIX. Timing starts and stops around the parsing only; corpus
// loading is amortized across benchmark iterations.
void BM_QuickFIX_SetString(benchmark::State& state) {
    const auto& msgs = split_corpus();
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

// Stream splitting only: feed bytes to FIX::Parser, extract each frame,
// but *don't* parse its tags. This is the apples-to-apples counterpart
// for a pre-parser that scans for frame boundaries without building a
// field index — use it when comparing scan throughput in isolation.
void BM_QuickFIX_StreamSplit(benchmark::State& state) {
    const auto& stream = stream_corpus();
    if (stream.empty()) {
        state.SkipWithError("stream is empty");
        return;
    }

    std::int64_t msg_count = 0;

    for (auto _ : state) {
        FIX::Parser parser;
        parser.addToStream(stream.data(), stream.size());

        std::string frame;
        std::int64_t iter_count = 0;
        try {
            while (parser.readFixMessage(frame)) {
                benchmark::DoNotOptimize(frame);
                ++iter_count;
            }
        } catch (const std::exception&) {
            state.SkipWithError("stream split threw");
            return;
        }
        msg_count = iter_count;
    }

    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                            msg_count);
    state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) *
                            static_cast<std::int64_t>(stream.size()));
}

// Stream splitting + full tag parsing. The stock on-the-wire path
// (continuous TCP bytes in, FIX::Message objects out) and the apples-
// to-apples baseline for a pre-parser that drives the shim all the way
// through to QuickFIX-compatible Message construction.
void BM_QuickFIX_StreamParse(benchmark::State& state) {
    const auto& stream = stream_corpus();
    if (stream.empty()) {
        state.SkipWithError("stream is empty");
        return;
    }

    std::int64_t msg_count = 0;

    for (auto _ : state) {
        FIX::Parser parser;
        parser.addToStream(stream.data(), stream.size());

        std::string frame;
        std::int64_t iter_count = 0;
        try {
            while (parser.readFixMessage(frame)) {
                FIX::Message parsed;
                parsed.setString(frame, /*validate=*/false);
                benchmark::DoNotOptimize(parsed);
                ++iter_count;
            }
        } catch (const std::exception&) {
            state.SkipWithError("stream parse threw");
            return;
        }
        msg_count = iter_count;
    }

    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                            msg_count);
    state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) *
                            static_cast<std::int64_t>(stream.size()));
}

auto p50 = [](const std::vector<double>& v) {
    auto sorted = v;
    std::nth_element(sorted.begin(),
                     sorted.begin() + static_cast<std::ptrdiff_t>(sorted.size() / 2),
                     sorted.end());
    return sorted[sorted.size() / 2];
};

auto p99 = [](const std::vector<double>& v) {
    auto sorted = v;
    std::size_t idx = (sorted.size() * 99) / 100;
    if (idx >= sorted.size()) idx = sorted.size() - 1;
    std::nth_element(sorted.begin(),
                     sorted.begin() + static_cast<std::ptrdiff_t>(idx),
                     sorted.end());
    return sorted[idx];
};

void configure(benchmark::internal::Benchmark* b) {
    b->Unit(benchmark::kNanosecond)
     ->ComputeStatistics("p50", p50)
     ->ComputeStatistics("p99", p99)
     ->Repetitions(10)
     ->ReportAggregatesOnly(false)
     ->DisplayAggregatesOnly(false);
}

}  // namespace

void register_benchmarks() {
    // Auto-detect corpus layout and register whichever benchmark makes
    // sense for it. A file → stream mode; a directory → per-file mode.
    // Both registrations in one binary would be misleading — only one of
    // them matches what the user pointed us at.
    const auto path = resolve_corpus_path();
    if (std::filesystem::is_regular_file(path)) {
        configure(benchmark::RegisterBenchmark(
            "QuickFIX_StreamSplit", BM_QuickFIX_StreamSplit));
        configure(benchmark::RegisterBenchmark(
            "QuickFIX_StreamParse", BM_QuickFIX_StreamParse));
    } else {
        configure(benchmark::RegisterBenchmark(
            "QuickFIX_SetString", BM_QuickFIX_SetString));
    }
}

}  // namespace swiftfix::bench
