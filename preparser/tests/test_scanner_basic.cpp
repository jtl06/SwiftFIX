// test_scanner_basic.cpp — Phase 1 smoke test.
//
// Confirms that the preparser headers compile, symbols link, and the
// dispatcher returns a non-null scalar scanner. Deeper parsing coverage
// (header stages, body loop, checksum, fallback cases) lives in
// test_scanner_scalar.cpp when that file lands.
#include <cstddef>
#include <span>

#include <gtest/gtest.h>

#include "swiftfix/scanner.hpp"
#include "swiftfix/status.hpp"

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

TEST(ScannerBasic, Avx2ScannerIsAddressable) {
    auto* s = swiftfix::scanner_for(swiftfix::ScannerKind::Avx2);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->kind(), swiftfix::ScannerKind::Avx2);
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

TEST(ScannerBasic, StatusToStringIsNotEmpty) {
    EXPECT_FALSE(swiftfix::to_string(swiftfix::ScanStatus::Ok).empty());
    EXPECT_FALSE(swiftfix::to_string(swiftfix::ScanStatus::Malformed).empty());
}
