// scanner_avx2.cpp — AVX2 SIMD implementation.
//
// Phase 0: not implemented. The translation unit exists so the build graph
// is stable once dispatch starts selecting this path. Runtime dispatch in
// preparser/src/dispatch.cpp will currently never return this scanner
// because kind_is_available() reports false.
#include "swiftfix/scanner.hpp"

namespace swiftfix {

bool avx2_is_available_at_runtime() noexcept {
    // TODO(phase2): runtime CPUID check. For now, report unavailable so the
    // dispatcher always picks the scalar path.
    return false;
}

Scanner* avx2_scanner_instance() noexcept {
    // TODO(phase2): return a real singleton once implemented.
    return nullptr;
}

}  // namespace swiftfix
