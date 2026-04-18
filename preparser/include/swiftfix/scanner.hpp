// scanner.hpp — public Scanner API.
//
// A Scanner consumes a raw buffer of FIX bytes and produces a FieldIndex.
// The library ships a scalar reference implementation plus ISA-specific
// SIMD implementations; `default_scanner()` returns whichever one is best
// for the running CPU (see preparser/src/dispatch.cpp).
//
// All scanners are stateless across calls — the FieldIndex out-param carries
// state. Buffer lifetime is the caller's; the scanner only records offsets.
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "swiftfix/field_index.hpp"
#include "swiftfix/status.hpp"

namespace swiftfix {

// Tagged identifier for the scan implementation actually in use. Exposed so
// tests and dispatch diagnostics can assert the right code path ran.
enum class ScannerKind : std::uint8_t {
    Scalar,
    Avx2,
    Avx512,
    Neon,
};

class Scanner {
  public:
    virtual ~Scanner() = default;

    // Scan a single FIX frame starting at the beginning of `buffer`. On
    // ScanStatus::Ok, `out` is fully populated and `out.frame_length` tells
    // the caller how many bytes were consumed. On non-Ok status, `out` may
    // be partially populated and must not be read.
    virtual ScanStatus scan(std::span<const std::byte> buffer,
                            FieldIndex& out) noexcept = 0;

    virtual ScannerKind kind() const noexcept = 0;
};

// Return the best scanner for the running CPU. The pointer refers to a
// process-lifetime singleton; callers must not delete it. Thread-safe.
Scanner& default_scanner() noexcept;

// Return a specific implementation regardless of the running CPU — useful
// for tests that need to exercise the scalar path on an AVX2 host. Returns
// nullptr if the requested kind is not compiled in.
Scanner* scanner_for(ScannerKind kind) noexcept;

}  // namespace swiftfix
