cmake_minimum_required(VERSION 3.20)

project(SwiftFIX
    VERSION 0.0.1
    DESCRIPTION "SIMD-assisted FIX pre-parser for QuickFIX C++"
    LANGUAGES CXX C)

# --- Global settings -------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Default build type" FORCE)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# --- Project options -------------------------------------------------------

option(SWIFTFIX_BUILD_TESTS       "Build unit tests"                 ON)
option(SWIFTFIX_BUILD_BENCH       "Build benchmark harness"          ON)
option(SWIFTFIX_BUILD_INTEGRATION "Build QuickFIX integration shim"  ON)
option(SWIFTFIX_ENABLE_SANITIZERS "Enable ASan+UBSan (Debug only)"   OFF)

# --- CMake helpers ---------------------------------------------------------

include(CompilerFlags)
include(Sanitizers)
include(SimdDetection)

find_package(Threads REQUIRED)

# --- QuickFIX (vendored submodule, built via ExternalProject) --------------

include(ExternalProject)

set(QUICKFIX_SRC_DIR    "${CMAKE_SOURCE_DIR}/third_party/quickfix")
set(QUICKFIX_BUILD_DIR  "${CMAKE_BINARY_DIR}/quickfix-build")
set(QUICKFIX_INSTALL_DIR "${CMAKE_BINARY_DIR}/quickfix-install")

if(NOT EXISTS "${QUICKFIX_SRC_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "QuickFIX submodule not initialized. Run:\n"
        "  git submodule update --init --recursive")
endif()

# QuickFIX's upstream CMake unconditionally builds a SHARED library on
# non-Windows platforms (and a static one on Windows) — there's no option
# to control it. We link against the shared library and embed an RPATH so
# consumers don't need LD_LIBRARY_PATH at runtime.
set(QUICKFIX_SONAME "libquickfix.so.16")
set(QUICKFIX_LIB_PATH "${QUICKFIX_INSTALL_DIR}/lib/${QUICKFIX_SONAME}")

ExternalProject_Add(quickfix_external
    SOURCE_DIR       "${QUICKFIX_SRC_DIR}"
    BINARY_DIR       "${QUICKFIX_BUILD_DIR}"
    INSTALL_DIR      "${QUICKFIX_INSTALL_DIR}"
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_INSTALL_PREFIX=${QUICKFIX_INSTALL_DIR}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5
        # QuickFIX v1.15.1 still uses dynamic exception specifications
        # (`throw(X)`), which C++17 removed. Pin its sub-build to C++14.
        # This is local to the QuickFIX build only — the rest of SwiftFIX
        # is still C++20.
        -DCMAKE_CXX_STANDARD=14
        -DCMAKE_CXX_STANDARD_REQUIRED=ON
        -DCMAKE_CXX_EXTENSIONS=OFF
        # Match parent-project toolchain so mixing compilers doesn't
        # cause link-time surprises.
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DHAVE_SSL=OFF
        -DHAVE_MYSQL=OFF
        -DHAVE_POSTGRESQL=OFF
        -DHAVE_PYTHON=OFF
    BUILD_BYPRODUCTS "${QUICKFIX_LIB_PATH}"
    UPDATE_DISCONNECTED 1
    LOG_DOWNLOAD 1
    LOG_CONFIGURE 1
    LOG_BUILD 1
    LOG_INSTALL 1)

file(MAKE_DIRECTORY "${QUICKFIX_INSTALL_DIR}/include")

# Proper imported target. IMPORTED_LOCATION is the file to link against;
# INTERFACE_INCLUDE_DIRECTORIES provides header visibility to consumers.
add_library(SwiftFIX_QuickFIX SHARED IMPORTED GLOBAL)
add_library(SwiftFIX::QuickFIX ALIAS SwiftFIX_QuickFIX)
add_dependencies(SwiftFIX_QuickFIX quickfix_external)
set_target_properties(SwiftFIX_QuickFIX PROPERTIES
    IMPORTED_LOCATION "${QUICKFIX_LIB_PATH}"
    INTERFACE_INCLUDE_DIRECTORIES "${QUICKFIX_INSTALL_DIR}/include"
    INTERFACE_LINK_LIBRARIES "Threads::Threads")

# Embed RPATH into downstream executables so they resolve libquickfix.so
# out of the build tree without LD_LIBRARY_PATH.
list(APPEND CMAKE_BUILD_RPATH "${QUICKFIX_INSTALL_DIR}/lib")
set(CMAKE_INSTALL_RPATH "${QUICKFIX_INSTALL_DIR}/lib")

# --- Subdirectories --------------------------------------------------------

add_subdirectory(preparser)

if(SWIFTFIX_BUILD_INTEGRATION)
    add_subdirectory(integration/shim)
endif()

if(SWIFTFIX_BUILD_BENCH)
    add_subdirectory(bench)
endif()

# --- Tests -----------------------------------------------------------------

if(SWIFTFIX_BUILD_TESTS)
    enable_testing()
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_subdirectory(preparser/tests)
    if(SWIFTFIX_BUILD_INTEGRATION)
        add_subdirectory(integration/tests)
    endif()
endif()

# --- Summary ---------------------------------------------------------------

message(STATUS "")
message(STATUS "SwiftFIX configuration:")
message(STATUS "  Build type:          ${CMAKE_BUILD_TYPE}")
message(STATUS "  Tests:               ${SWIFTFIX_BUILD_TESTS}")
message(STATUS "  Bench:               ${SWIFTFIX_BUILD_BENCH}")
message(STATUS "  Integration shim:    ${SWIFTFIX_BUILD_INTEGRATION}")
message(STATUS "  Sanitizers:          ${SWIFTFIX_ENABLE_SANITIZERS}")
message(STATUS "  AVX2 detected:       ${SWIFTFIX_HAVE_AVX2}")
message(STATUS "  AVX-512 detected:    ${SWIFTFIX_HAVE_AVX512}")
message(STATUS "  NEON detected:       ${SWIFTFIX_HAVE_NEON}")
message(STATUS "  QuickFIX install:    ${QUICKFIX_INSTALL_DIR}")
message(STATUS "")
