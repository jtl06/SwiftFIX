// test_scanner_basic.cpp — scanner dispatch and status smoke tests.
//
// Confirms that the preparser headers compile, symbols link, and the
// dispatcher returns a usable scalar or AVX2 scanner.
#include <cstddef>
#include <span>

#include <gtest/gtest.h>

#include "swiftfix/scanner.hpp"
#include "swiftfix/status.hpp"

namespace swiftfix {
// Defined in preparser/src/scanner_avx2.cpp; not in the public header.
bool avx2_is_available_at_runtime() noexcept;
}  // namespace swiftfix

TEST(ScannerBasic, DefaultScannerReturnsScalarOrAvx2) {
    auto& s = swiftfix::default_scanner();
    EXPECT_TRUE(s.kind() == swiftfix::ScannerKind::Scalar ||
                s.kind() == swiftfix::ScannerKind::Avx2);
}

TEST(ScannerBasic, ScalarScannerIsAddressable) {
    auto* s = swiftfix::scanner_for(swiftfix::ScannerKind::Scalar);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->kind(), swiftfix::ScannerKind::Scalar);
}

TEST(ScannerBasic, Avx2ScannerMatchesRuntimeProbe) {
    auto* s = swiftfix::scanner_for(swiftfix::ScannerKind::Avx2);
    if (swiftfix::avx2_is_available_at_runtime()) {
        ASSERT_NE(s, nullptr);
        EXPECT_EQ(s->kind(), swiftfix::ScannerKind::Avx2);
    } else {
        EXPECT_EQ(s, nullptr);
    }
}

TEST(ScannerBasic, EmptyBufferIsTruncated) {
    // An empty buffer is consistent with a frame in progress — the scanner
    // must report Truncated so the caller can accumulate more bytes rather
    // than treating it as malformed.
    auto& s = swiftfix::default_scanner();
    swiftfix::FieldIndex idx;
    std::byte buf[1]{};
    auto status = s.scan(std::span<const std::byte>(buf, 0), idx);
    EXPECT_EQ(status, swiftfix::ScanStatus::Truncated);
}

TEST(ScannerBasic, TruncatedAfterBeginStringIsTruncated) {
    const char raw[] = "8=FIX.4.4EXTRA\x01";
    auto& s = swiftfix::default_scanner();
    swiftfix::FieldIndex idx;
    const auto status = s.scan(
        std::span<const std::byte>(
            reinterpret_cast<const std::byte*>(raw), sizeof(raw) - 1),
        idx);
    EXPECT_EQ(status, swiftfix::ScanStatus::Truncated);
}

TEST(ScannerBasic, StatusToStringIsNotEmpty) {
    EXPECT_FALSE(swiftfix::to_string(swiftfix::ScanStatus::Ok).empty());
    EXPECT_FALSE(swiftfix::to_string(swiftfix::ScanStatus::Malformed).empty());
}
