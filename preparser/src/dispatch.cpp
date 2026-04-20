// dispatch.cpp — runtime ISA selection.
//
// `default_scanner()` consults runtime CPUID probes (provided by the
// per-ISA translation units) and returns the best available scanner.
// `scanner_for()` is the testing backdoor that bypasses the probe.
//
#include "swiftfix/scanner.hpp"

#ifndef SWIFTFIX_COMPILE_AVX2
#define SWIFTFIX_COMPILE_AVX2 0
#endif

namespace swiftfix {

// Forward declarations from per-ISA TUs.
Scanner& scalar_scanner_instance() noexcept;
#if SWIFTFIX_COMPILE_AVX2
Scanner* avx2_scanner_instance() noexcept;
bool avx2_is_available_at_runtime() noexcept;
#else
bool avx2_is_available_at_runtime() noexcept {
    return false;
}
#endif

Scanner& default_scanner() noexcept {
    // Best-ISA selection is computed once and cached. Thread-safe under
    // C++20 "magic statics".
    static Scanner& chosen = []() -> Scanner& {
#if SWIFTFIX_COMPILE_AVX2
        if (avx2_is_available_at_runtime()) {
            if (auto* s = avx2_scanner_instance()) {
                return *s;
            }
        }
#endif
        return scalar_scanner_instance();
    }();
    return chosen;
}

Scanner* scanner_for(ScannerKind kind) noexcept {
    switch (kind) {
        case ScannerKind::Scalar:
            return &scalar_scanner_instance();
        case ScannerKind::Avx2:
#if SWIFTFIX_COMPILE_AVX2
            return avx2_is_available_at_runtime() ? avx2_scanner_instance()
                                                  : nullptr;
#else
            return nullptr;
#endif
        case ScannerKind::Avx512:
        case ScannerKind::Neon:
            return nullptr;
    }
    return nullptr;
}

}  // namespace swiftfix
