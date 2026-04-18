# Integration tests — preparser + QuickFIX, end-to-end.

add_executable(swiftfix_integration_tests
    test_end_to_end.cpp)

target_link_libraries(swiftfix_integration_tests
    PRIVATE
        SwiftFIX::shim
        SwiftFIX::CompilerFlags
        SwiftFIX::Sanitizers
        GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(swiftfix_integration_tests)
