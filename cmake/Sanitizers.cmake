# Sanitizers.cmake
#
# Address + Undefined Behavior sanitizers for Debug builds.
# Opt in via -DSWIFTFIX_ENABLE_SANITIZERS=ON. Applied to every target that
# links swiftfix_sanitizers.

add_library(swiftfix_sanitizers INTERFACE)
add_library(SwiftFIX::Sanitizers ALIAS swiftfix_sanitizers)

if(SWIFTFIX_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(swiftfix_sanitizers INTERFACE
            -fsanitize=address,undefined
            -fno-sanitize-recover=all
            -fno-omit-frame-pointer)
        target_link_options(swiftfix_sanitizers INTERFACE
            -fsanitize=address,undefined)
        message(STATUS "Sanitizers: address + undefined enabled")
    else()
        message(WARNING "Sanitizers requested but unsupported on ${CMAKE_CXX_COMPILER_ID}")
    endif()
endif()
