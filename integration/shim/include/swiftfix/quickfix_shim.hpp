// quickfix_shim.hpp — public API for feeding a pre-scanned FieldIndex into
// QuickFIX. The shim is behind a feature flag; by default every call falls
// through to stock QuickFIX parsing (see docs/fallback_policy.md).
#pragma once

#include <cstddef>
#include <span>
#include <string>

#include "swiftfix/field_index.hpp"
#include "swiftfix/status.hpp"

// Forward declarations to keep <quickfix/Message.h> out of this header.
namespace FIX { class Message; }

namespace swiftfix::shim {

// Set globally at startup. When false (the default), every entry point in
// this header immediately falls back to stock QuickFIX parsing. When true,
// the shim tries the pre-parser first and falls back on any non-Ok status.
void set_preparse_enabled(bool on) noexcept;
bool preparse_enabled() noexcept;

// Populate a FIX::Message from a raw byte buffer. This is the primary
// entry point the patched QuickFIX Session calls instead of
// Message::setString. On any non-Ok ScanStatus the shim internally falls
// back to the stock path and still returns true.
//
// Returns true if the message was successfully parsed (by either path),
// false if even the stock path rejects the frame.
bool parse_into(std::span<const std::byte> buffer, FIX::Message& out);

// Diagnostics: how many frames went through each path since process start.
// Useful for verifying that the feature flag actually takes effect under
// load and for comparing fallback rates across corpus slices.
struct ShimStats {
    std::uint64_t preparse_attempted = 0;
    std::uint64_t preparse_succeeded = 0;
    std::uint64_t fallback_used      = 0;
    std::uint64_t parse_failed       = 0;
};

ShimStats snapshot_stats() noexcept;
void reset_stats() noexcept;

}  // namespace swiftfix::shim
