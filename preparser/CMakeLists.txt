# libswiftfix — the pre-parser library.

set(SWIFTFIX_PREPARSER_SOURCES
    src/scanner_scalar.cpp
    src/dispatch.cpp)

if(SWIFTFIX_HAVE_AVX2)
    list(APPEND SWIFTFIX_PREPARSER_SOURCES src/scanner_avx2.cpp)
    # AVX2 scanner needs -mavx2 regardless of global -march, so the TU
    # compiles on portable x86 builds. Runtime dispatch gates execution.
    set_source_files_properties(src/scanner_avx2.cpp
        PROPERTIES COMPILE_OPTIONS "-mavx2")
endif()

add_library(swiftfix STATIC ${SWIFTFIX_PREPARSER_SOURCES})

add_library(SwiftFIX::swiftfix ALIAS swiftfix)

target_include_directories(swiftfix PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_link_libraries(swiftfix
    PUBLIC
        SwiftFIX::CompilerFlags
    PRIVATE
        SwiftFIX::Sanitizers)

target_compile_definitions(swiftfix PRIVATE
    SWIFTFIX_COMPILE_AVX2=$<BOOL:${SWIFTFIX_HAVE_AVX2}>)

set_target_properties(swiftfix PROPERTIES
    OUTPUT_NAME swiftfix
    POSITION_INDEPENDENT_CODE ON)
