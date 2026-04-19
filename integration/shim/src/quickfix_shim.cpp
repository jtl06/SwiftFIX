// quickfix_shim.cpp — implementation.
//
// Phase 1: preparse is disabled by default and every parse_into call falls
// straight through to FIX::Message::setString. When the QuickFIX patch
// series lands, apply_field_index() gets a real body and the preparse path
// becomes the fast path. See integration/patches/README.md for the patch
// plan and docs/fallback_policy.md for when we fall back.
#include "swiftfix/quickfix_shim.hpp"

#include <atomic>
#include <string>

#include <quickfix/Message.h>

#include "swiftfix/scanner.hpp"

namespace swiftfix::shim {

namespace {

std::atomic<bool> g_preparse_enabled{false};

bool stock_parse(std::span<const std::byte> buffer, FIX::Message& out) {
    try {
        std::string raw(reinterpret_cast<const char*>(buffer.data()),
                        buffer.size());
        out.setString(raw, /*validate=*/false);
        return true;
    } catch (...) {
        return false;
    }
}

// Translate FieldEntry (offset-based) to PreScanField (pointer-based) in
// `scratch`, then hand the array to FIX::Message::setFromPreScan. Returns
// false if QuickFIX rejected the field list, so the caller falls back.
bool apply_field_index(const FieldIndex& idx,
                       std::span<const std::byte> buffer,
                       FIX::Message& out,
                       FIX::PreScanField* scratch) {
    const char* const base = reinterpret_cast<const char*>(buffer.data());
    for (std::uint32_t i = 0; i < idx.field_count; ++i) {
        const auto& e = idx.fields[i];
        scratch[i] = FIX::PreScanField{
            static_cast<int>(e.tag_number),
            base + e.tag_start,
            base + e.value_start,
            base + e.value_end,
        };
    }
    try {
        out.setFromPreScan(scratch, idx.field_count, /*doValidation=*/false);
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace

void set_preparse_enabled(bool on) noexcept {
    g_preparse_enabled.store(on, std::memory_order_release);
}

bool preparse_enabled() noexcept {
    return g_preparse_enabled.load(std::memory_order_acquire);
}

bool SessionShim::parse_into(std::span<const std::byte> buffer,
                             FIX::Message& out) {
    if (!preparse_enabled()) {
        bool ok = stock_parse(buffer, out);
        if (!ok) ++stats_.parse_failed;
        return ok;
    }

    ++stats_.preparse_attempted;
    const auto s = default_scanner().scan(buffer, idx_);
    if (s == ScanStatus::Ok &&
        apply_field_index(idx_, buffer, out, scratch_.data())) {
        ++stats_.preparse_succeeded;
        return true;
    }
    ++stats_.fallback_used;
    const bool ok = stock_parse(buffer, out);
    if (!ok) ++stats_.parse_failed;
    return ok;
}

}  // namespace swiftfix::shim
