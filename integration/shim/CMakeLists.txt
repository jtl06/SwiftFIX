# swiftfix_shim — bridges libswiftfix to QuickFIX. Depends on both.

add_library(swiftfix_shim STATIC
    src/quickfix_shim.cpp)

add_library(SwiftFIX::shim ALIAS swiftfix_shim)

target_include_directories(swiftfix_shim PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_link_libraries(swiftfix_shim
    PUBLIC
        SwiftFIX::swiftfix
        SwiftFIX::QuickFIX
        SwiftFIX::CompilerFlags
    PRIVATE
        SwiftFIX::Sanitizers)
