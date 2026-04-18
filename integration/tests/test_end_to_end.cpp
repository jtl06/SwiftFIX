// test_end_to_end.cpp — Phase 0 smoke tests for the shim.
//
// Verifies: with preparse disabled (the default) the shim parses a valid
// FIX frame via stock QuickFIX, and the stats counters move in the
// expected direction. Does not yet exercise the preparse path — that
// lands when the scalar scanner is real.
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
    swiftfix::shim::reset_stats();
    swiftfix::shim::set_preparse_enabled(false);

    FIX::Message msg;
    bool ok = swiftfix::shim::parse_into(
        as_bytes(kLogon, sizeof(kLogon) - 1), msg);

    EXPECT_TRUE(ok);
    EXPECT_EQ(swiftfix::shim::snapshot_stats().parse_failed, 0u);
}

TEST(Shim, StockPathRejectsGarbage) {
    swiftfix::shim::reset_stats();
    swiftfix::shim::set_preparse_enabled(false);

    const char garbage[] = "not a fix message";
    FIX::Message msg;
    bool ok = swiftfix::shim::parse_into(
        as_bytes(garbage, sizeof(garbage) - 1), msg);

    EXPECT_FALSE(ok);
    EXPECT_EQ(swiftfix::shim::snapshot_stats().parse_failed, 1u);
}

TEST(Shim, EnablingPreparseStillFallsBackInPhase0) {
    // With the scanner stubbed as FallbackRequested, enabling the flag
    // should not produce a different result than the stock path.
    swiftfix::shim::reset_stats();
    swiftfix::shim::set_preparse_enabled(true);

    FIX::Message msg;
    bool ok = swiftfix::shim::parse_into(
        as_bytes(kLogon, sizeof(kLogon) - 1), msg);

    EXPECT_TRUE(ok);
    EXPECT_GE(swiftfix::shim::snapshot_stats().fallback_used, 1u);

    swiftfix::shim::set_preparse_enabled(false);
}
