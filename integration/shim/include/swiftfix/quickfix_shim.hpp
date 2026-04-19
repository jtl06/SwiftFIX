// quickfix_shim.hpp — public API for feeding a pre-scanned FieldIndex into
// QuickFIX. The shim is behind a feature flag; by default every call falls
// through to stock QuickFIX parsing (see docs/fallback_policy.md).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <quickfix/Message.h>

#include "swiftfix/field_index.hpp"
#include "swiftfix/status.hpp"

namespace swiftfix::shim {

// Set globally at startup. When false (the default), every SessionShim
// parse_into() call immediately falls back to stock QuickFIX parsing. When
// true, the shim tries the pre-parser first and falls back on any non-Ok
// status.
void set_preparse_enabled(bool on) noexcept;
bool preparse_enabled() noexcept;

// Per-session state. Holds the reusable FieldIndex so scan() runs
// zero-alloc in steady state — see docs/field_index_format.md
// (Lifetime & ownership) for the rationale.
//
// Not thread-safe. QuickFIX's Session model already gives each session a
// single-threaded reader; instantiate one SessionShim per Session. Tests
// and benches that want parallelism instantiate per thread.
class SessionShim {
  public:
    // Populate `out` from `buffer`. Returns true if parsing succeeded by
    // either path, false if even the stock path rejects the frame. On any
    // non-Ok pre-parser status, falls back internally to stock QuickFIX.
    bool parse_into(std::span<const std::byte> buffer, FIX::Message& out);

    // Diagnostics — useful for verifying the feature flag actually takes
    // effect under load and for comparing fallback rates across corpus
    // slices. Counts are per-instance; aggregate in the caller if desired.
    struct Stats {
        std::uint64_t preparse_attempted = 0;
        std::uint64_t preparse_succeeded = 0;
        std::uint64_t fallback_used      = 0;
        std::uint64_t parse_failed       = 0;
    };

    const Stats& stats() const noexcept { return stats_; }
    void reset_stats() noexcept { stats_ = {}; }

  private:
    FieldIndex idx_{};
    // Scratch storage for the FieldIndex → PreScanField translation.
    // Sized to kMaxFields so it matches FieldIndex.fields[] one-for-one;
    // kept per-session to stay zero-alloc in steady state.
    std::array<FIX::PreScanField, kMaxFields> scratch_{};
    Stats stats_{};
};

}  // namespace swiftfix::shim
