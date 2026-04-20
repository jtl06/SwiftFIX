# libswiftfix — the pre-parser library.

add_library(swiftfix STATIC
    src/scanner_scalar.cpp
    src/scanner_avx2.cpp
    src/dispatch.cpp)

# AVX2 scanner needs -mavx2 regardless of global -march, so the TU compiles
# on portable builds. Runtime dispatch in dispatch.cpp gates execution on
# __builtin_cpu_supports("avx2") so non-AVX2 hosts never enter this path.
set_source_files_properties(src/scanner_avx2.cpp
    PROPERTIES COMPILE_OPTIONS "-mavx2")

add_library(SwiftFIX::swiftfix ALIAS swiftfix)

target_include_directories(swiftfix PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_link_libraries(swiftfix
    PUBLIC
        SwiftFIX::CompilerFlags
    PRIVATE
        SwiftFIX::Sanitizers)

set_target_properties(swiftfix PROPERTIES
    OUTPUT_NAME swiftfix
    POSITION_INDEPENDENT_CODE ON)
