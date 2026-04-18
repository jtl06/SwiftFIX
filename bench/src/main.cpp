#include <benchmark/benchmark.h>

#include "harness.hpp"

int main(int argc, char** argv) {
    ::benchmark::Initialize(&argc, argv);
    swiftfix::bench::register_benchmarks();
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();
    return 0;
}
