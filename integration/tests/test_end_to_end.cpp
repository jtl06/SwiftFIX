// test_end_to_end.cpp — Phase 1 smoke tests for the shim.
//
// Verifies: with preparse disabled (the default) a SessionShim parses a
// valid FIX frame via stock QuickFIX, and the stats counters move in the
// expected direction. With preparse enabled and the QuickFIX patch series
// applied, apply_field_index drives the fast path; the test asserts
// preparse_succeeded advances and no fallback is taken.
#include <array>
#include <cstddef>
#include <cstring>
#include <span>
#include <string>

#include <gtest/gtest.h>
#include <quickfix/Message.h>

#include "swiftfix/quickfix_shim.hpp"

namespace {

// A canned FIX Logon message matching corpus/valid/01_logon byte-for-byte.
// Hand-pinning one here keeps this test independent of the on-disk corpus;
// if you regenerate the corpus and the BodyLength/CheckSum shift, update
// this literal to match.
constexpr const char kLogon[] =
    "8=FIX.4.4\x01"
    "9=67\x01"
    "35=A\x01"
    "49=SENDER\x01"
    "56=TARGET\x01"
    "34=1\x01"
    "52=20260101-00:00:00.000\x01"
    "98=0\x01"
    "108=30\x01"
    "10=103\x01";

std::span<const std::byte> as_bytes(const char* s, std::size_t n) {
    return {reinterpret_cast<const std::byte*>(s), n};
}

}  // namespace

TEST(Shim, DefaultsToPreparseDisabled) {
    EXPECT_FALSE(swiftfix::shim::preparse_enabled());
}

TEST(Shim, StockPathParsesValidLogon) {
    swiftfix::shim::set_preparse_enabled(false);
    swiftfix::shim::SessionShim shim;

    FIX::Message msg;
    const bool ok = shim.parse_into(
        as_bytes(kLogon, sizeof(kLogon) - 1), msg);

    EXPECT_TRUE(ok);
    EXPECT_EQ(shim.stats().parse_failed, 0u);
    EXPECT_EQ(shim.stats().preparse_attempted, 0u);
}

TEST(Shim, StockPathRejectsGarbage) {
    swiftfix::shim::set_preparse_enabled(false);
    swiftfix::shim::SessionShim shim;

    const char garbage[] = "not a fix message";
    FIX::Message msg;
    const bool ok = shim.parse_into(
        as_bytes(garbage, sizeof(garbage) - 1), msg);

    EXPECT_FALSE(ok);
    EXPECT_EQ(shim.stats().parse_failed, 1u);
}

TEST(Shim, EnablingPreparseTakesFastPath) {
    // With patch 0001 applied, apply_field_index drives setFromPreScan and
    // the shim should succeed on the fast path without falling back.
    swiftfix::shim::set_preparse_enabled(true);
    swiftfix::shim::SessionShim shim;

    FIX::Message msg;
    const bool ok = shim.parse_into(
        as_bytes(kLogon, sizeof(kLogon) - 1), msg);

    EXPECT_TRUE(ok);
    EXPECT_EQ(shim.stats().preparse_attempted, 1u);
    EXPECT_EQ(shim.stats().preparse_succeeded, 1u);
    EXPECT_EQ(shim.stats().fallback_used, 0u);
    EXPECT_EQ(shim.stats().parse_failed, 0u);

    swiftfix::shim::set_preparse_enabled(false);
}
