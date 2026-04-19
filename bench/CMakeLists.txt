# bench/ — Google Benchmark harness for replaying FIX bytes through
# stock QuickFIX (no pre-parser). Phase 0 produces the baseline numbers
# every future optimization is measured against.

include(FetchContent)

# Optional libpfm4 integration: if the dev headers are present we let
# Google Benchmark use them for --benchmark_perf_counters=... . That
# gives us machine-independent signals (INSTRUCTIONS, BRANCHES,
# BRANCH-MISSES, CYCLES, CACHE-*), which beat wall-clock ns for
# cross-machine comparability.
#
# To install on Debian/Ubuntu:  sudo apt install libpfm4-dev
# And ensure PMU events are readable:
#   sudo sysctl kernel.perf_event_paranoid=1
find_path(SWIFTFIX_LIBPFM_INCLUDE_DIR perfmon/pfmlib.h)
find_library(SWIFTFIX_LIBPFM_LIBRARY NAMES pfm)
if(SWIFTFIX_LIBPFM_INCLUDE_DIR AND SWIFTFIX_LIBPFM_LIBRARY)
    set(BENCHMARK_ENABLE_LIBPFM ON CACHE BOOL "" FORCE)
    message(STATUS "  libpfm4 headers:     ${SWIFTFIX_LIBPFM_INCLUDE_DIR}")
    message(STATUS "  libpfm4 library:     ${SWIFTFIX_LIBPFM_LIBRARY}")
    message(STATUS "  perf counters:       enabled (use --benchmark_perf_counters=...)")
else()
    set(BENCHMARK_ENABLE_LIBPFM OFF CACHE BOOL "" FORCE)
    message(STATUS "  libpfm4 headers:     NOT FOUND")
    message(STATUS "  perf counters:       disabled — install libpfm4-dev to enable")
endif()

FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googlebenchmark)

add_executable(swiftfix_bench
    src/main.cpp
    src/harness.cpp
    src/replay.cpp)

target_include_directories(swiftfix_bench PRIVATE src)

target_link_libraries(swiftfix_bench
    PRIVATE
        SwiftFIX::QuickFIX
        SwiftFIX::swiftfix
        SwiftFIX::CompilerFlags
        benchmark::benchmark)

# The harness reads corpus bytes at runtime — bake the corpus directory
# into the binary so users don't have to pass it for the default case.
target_compile_definitions(swiftfix_bench PRIVATE
    SWIFTFIX_CORPUS_DIR="${CMAKE_SOURCE_DIR}/corpus/valid")
