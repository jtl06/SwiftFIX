# CompilerFlags.cmake
#
# Portable defaults + opt-in release-tuning flags. Kept conservative so CI on
# foreign microarchitectures still passes.

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(WARNING
        "SwiftFIX has only been tested with GCC 11+ and Clang 14+; "
        "your compiler is ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}.")
endif()

add_library(swiftfix_compiler_flags INTERFACE)
add_library(SwiftFIX::CompilerFlags ALIAS swiftfix_compiler_flags)

target_compile_features(swiftfix_compiler_flags INTERFACE cxx_std_20)

target_compile_options(swiftfix_compiler_flags INTERFACE
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Woverloaded-virtual
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
    >
)

# Release tuning — opt-in via -DSWIFTFIX_NATIVE=ON to bake -march=native into
# the build. CI keeps this off for portability; developer workstations and
# bench runs turn it on.
option(SWIFTFIX_NATIVE "Tune binaries for the host CPU (-march=native)" OFF)
if(SWIFTFIX_NATIVE)
    target_compile_options(swiftfix_compiler_flags INTERFACE
        $<$<CONFIG:Release,RelWithDebInfo>:-march=native>)
endif()

# Debug builds always need frame pointers for perf/flame graph readability.
target_compile_options(swiftfix_compiler_flags INTERFACE
    $<$<AND:$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>,$<CONFIG:Debug>>:
        -fno-omit-frame-pointer
    >
)
