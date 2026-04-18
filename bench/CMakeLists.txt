# bench/ — Google Benchmark harness for replaying FIX bytes through
# stock QuickFIX (no pre-parser). Phase 0 produces the baseline numbers
# every future optimization is measured against.

include(FetchContent)
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
        SwiftFIX::CompilerFlags
        benchmark::benchmark)

# The harness reads corpus bytes at runtime — bake the corpus directory
# into the binary so users don't have to pass it for the default case.
target_compile_definitions(swiftfix_bench PRIVATE
    SWIFTFIX_CORPUS_DIR="${CMAKE_SOURCE_DIR}/corpus/valid")
