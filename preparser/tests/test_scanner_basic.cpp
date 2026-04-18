// test_scanner_basic.cpp — Phase 0 smoke test.
//
// Confirms that the preparser headers compile, symbols link, and the
// dispatcher returns a non-null scalar scanner. Does *not* exercise parsing
// — the scalar scanner is still a stub that returns FallbackRequested by
// design. Real parsing tests land when the scalar implementation does.
#include <cstddef>
#include <span>

#include <gtest/gtest.h>

#include "swiftfix/scanner.hpp"
#include "swiftfix/status.hpp"

TEST(ScannerBasic, DefaultScannerReturnsScalarForNow) {
    auto& s = swiftfix::default_scanner();
    EXPECT_EQ(s.kind(), swiftfix::ScannerKind::Scalar);
}

TEST(ScannerBasic, ScalarScannerIsAddressable) {
    auto* s = swiftfix::scanner_for(swiftfix::ScannerKind::Scalar);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->kind(), swiftfix::ScannerKind::Scalar);
}

TEST(ScannerBasic, Avx2ScannerNotWiredUpYet) {
    // Phase 0: AVX2 path is a stub. When phase 2 implements it, flip this.
    EXPECT_EQ(swiftfix::scanner_for(swiftfix::ScannerKind::Avx2), nullptr);
}

TEST(ScannerBasic, ScalarStubReportsFallback) {
    // The scalar scanner is declared but not implemented; it must return
    // FallbackRequested so callers defer to stock QuickFIX.
    auto& s = swiftfix::default_scanner();
    swiftfix::FieldIndex idx;
    std::byte buf[1]{};
    auto status = s.scan(std::span<const std::byte>(buf, 0), idx);
    EXPECT_EQ(status, swiftfix::ScanStatus::FallbackRequested);
}

TEST(ScannerBasic, StatusToStringIsNotEmpty) {
    EXPECT_FALSE(swiftfix::to_string(swiftfix::ScanStatus::Ok).empty());
    EXPECT_FALSE(swiftfix::to_string(swiftfix::ScanStatus::Malformed).empty());
}
