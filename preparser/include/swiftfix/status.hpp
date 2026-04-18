// status.hpp — outcome codes for the pre-parser scan path.
//
// The pre-parser is *advisory*. On any non-OK status the caller is expected
// to fall back to stock QuickFIX message parsing; see docs/fallback_policy.md.
#pragma once

#include <cstdint>
#include <string_view>

namespace swiftfix {

enum class ScanStatus : std::uint8_t {
    Ok = 0,

    // The buffer does not contain a complete FIX message yet. Not an error —
    // the caller should accumulate more bytes and retry.
    Truncated,

    // The buffer starts with something that is not a valid FIX begin-string
    // (tag 8), or contains a byte sequence that cannot be a FIX tag-value frame.
    Malformed,

    // Header tags (8, 9, 35) were not found in the expected canonical order.
    // QuickFIX itself will reject this, but flagging early lets us skip
    // building a field-boundary table.
    BadHeader,

    // Tag 9 (BodyLength) was present and parseable, but the byte count it
    // declares is inconsistent with the <SOH> structure of the frame.
    BadBodyLength,

    // The pre-parser refuses to handle this message and wants the caller to
    // fall back to stock QuickFIX parsing. Reserved for edge cases the scalar
    // fallback covers that the SIMD fast path does not (e.g. certain RawData
    // layouts). See docs/embedded_data.md.
    FallbackRequested,

    // Field-boundary table would exceed preconfigured capacity. The caller
    // should fall back; this is not necessarily a malformed frame.
    TableFull,
};

constexpr std::string_view to_string(ScanStatus s) noexcept {
    switch (s) {
        case ScanStatus::Ok:                return "Ok";
        case ScanStatus::Truncated:         return "Truncated";
        case ScanStatus::Malformed:         return "Malformed";
        case ScanStatus::BadHeader:         return "BadHeader";
        case ScanStatus::BadBodyLength:     return "BadBodyLength";
        case ScanStatus::FallbackRequested: return "FallbackRequested";
        case ScanStatus::TableFull:         return "TableFull";
    }
    return "Unknown";
}

}  // namespace swiftfix
