// scanner_scalar.cpp — reference scalar implementation.
//
// Phase 0: the scanner API is declared but not implemented. Stubs return
// ScanStatus::FallbackRequested so any accidental production use cleanly
// defers to stock QuickFIX rather than emitting garbage.
//
// Implementation will arrive after profiling (docs/design.md:Phase 0 Findings)
// confirms the scan path is actually hot in the target workload.
#include "swiftfix/scanner.hpp"

namespace swiftfix {

namespace {

class ScalarScanner final : public Scanner {
  public:
    ScanStatus scan(std::span<const std::byte> buffer,
                    FieldIndex& out) noexcept override {
        (void)buffer;
        out.reset();
        // TODO(phase1): implement the scalar reference scanner. This is the
        // baseline every SIMD path must match byte-for-byte via
        // scripts/verify_corpus.sh (to be written in phase 2).
        return ScanStatus::FallbackRequested;
    }

    ScannerKind kind() const noexcept override { return ScannerKind::Scalar; }
};

}  // namespace

Scanner& scalar_scanner_instance() noexcept {
    static ScalarScanner instance;
    return instance;
}

}  // namespace swiftfix
