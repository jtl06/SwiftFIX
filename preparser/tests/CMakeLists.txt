# preparser unit tests.

add_executable(swiftfix_unit_tests
    test_scanner_basic.cpp
    test_field_index.cpp
    test_scanner_corpus.cpp)

target_link_libraries(swiftfix_unit_tests
    PRIVATE
        SwiftFIX::swiftfix
        SwiftFIX::CompilerFlags
        SwiftFIX::Sanitizers
        GTest::gtest_main)

target_compile_definitions(swiftfix_unit_tests PRIVATE
    SWIFTFIX_CORPUS_VALID_DIR="${CMAKE_SOURCE_DIR}/corpus/valid"
    SWIFTFIX_BULK_STREAM="${CMAKE_SOURCE_DIR}/corpus/bulk.stream")

include(GoogleTest)
gtest_discover_tests(swiftfix_unit_tests)
