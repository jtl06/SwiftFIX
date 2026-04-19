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

    // The buffer ended before a complete frame could be proven. The bytes
    // seen so far are consistent with a FIX frame in progress — either the
    // declared body length extends past buffer.size(), or the checksum
    // field's terminating <SOH> has not yet been observed. Not an error:
    // the caller should accumulate more bytes and retry.
    Truncated,

    // The bytes examined cannot form a valid FIX tag-value frame regardless
    // of what follows. Examples: input does not start with "8=", a non-digit
    // byte appears where a tag digit is required, '=' is missing from a
    // tag/value pair, or <SOH> is missing where it is required.
    Malformed,

    // The frame is structurally a tag-value sequence, but the canonical
    // header ordering (tag 8 then 9 then 35, all three present) was not
    // observed. QuickFIX would reject such a frame; flagging it early lets
    // us skip building a field-boundary table.
    BadHeader,

    // Tag 9 (BodyLength) parsed as a non-negative integer, but the measured
    // body span — bytes from the <SOH> after tag 9 up to the <SOH> before
    // the checksum field — disagrees with the declared length.
    BadBodyLength,

    // Scanner policy chose not to emit an index for this frame. This is NOT
    // a verdict on the frame's validity — the frame may be well-formed FIX
    // that the scanner (scalar or SIMD) declines to process in the current
    // phase. Example: scalar phase 1 returns FallbackRequested when it
    // encounters a length-prefixed embedded-data tag (95/96) rather than
    // hardcoding skip logic for every variant; a future SIMD path may
    // return it for constructs its kernel cannot handle. The status is
    // shared across scanner kinds — what triggers it differs per kind.
    // See docs/fallback_policy.md and docs/embedded_data.md.
    FallbackRequested,

    // The frame exceeded the inline field-table capacity (kMaxFields) before
    // the scanner reached the end. Resource limit, not a policy choice: the
    // message may be perfectly valid FIX, but this scanner cannot represent
    // it. Caller falls back to stock parsing.
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
