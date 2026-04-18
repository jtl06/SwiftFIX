# preparser unit tests.

add_executable(swiftfix_unit_tests
    test_scanner_basic.cpp
    test_field_index.cpp)

target_link_libraries(swiftfix_unit_tests
    PRIVATE
        SwiftFIX::swiftfix
        SwiftFIX::CompilerFlags
        SwiftFIX::Sanitizers
        GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(swiftfix_unit_tests)
