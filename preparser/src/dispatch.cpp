// dispatch.cpp — runtime ISA selection + thread-local arena management.
//
// `default_scanner()` consults runtime CPUID probes (provided by the
// per-ISA translation units) and returns the best available scanner.
// `scanner_for()` is the testing backdoor that bypasses the probe.
//
// Phase 0: only the scalar path is wired up. SIMD probes exist as TODO
// stubs in scanner_avx2.cpp etc.
#include "swiftfix/scanner.hpp"

namespace swiftfix {

// Forward declarations from per-ISA TUs.
Scanner& scalar_scanner_instance() noexcept;
Scanner* avx2_scanner_instance() noexcept;
bool avx2_is_available_at_runtime() noexcept;

Scanner& default_scanner() noexcept {
    // Best-ISA selection is computed once and cached. Thread-safe under
    // C++20 "magic statics".
    static Scanner& chosen = []() -> Scanner& {
        if (avx2_is_available_at_runtime()) {
            if (auto* s = avx2_scanner_instance()) {
                return *s;
            }
        }
        return scalar_scanner_instance();
    }();
    return chosen;
}

Scanner* scanner_for(ScannerKind kind) noexcept {
    switch (kind) {
        case ScannerKind::Scalar:
            return &scalar_scanner_instance();
        case ScannerKind::Avx2:
            return avx2_is_available_at_runtime() ? avx2_scanner_instance()
                                                  : nullptr;
        case ScannerKind::Avx512:
        case ScannerKind::Neon:
            // TODO(phase2/3): wire up once implementations exist.
            return nullptr;
    }
    return nullptr;
}

}  // namespace swiftfix
