// quickfix_shim.cpp — implementation stubs.
//
// Phase 0: preparse is disabled by default and every call falls straight
// through to FIX::Message::setString. Once the scalar scanner lands and
// the QuickFIX patch series is in place, this file wires them together.
#include "swiftfix/quickfix_shim.hpp"

#include <atomic>
#include <string>

#include <quickfix/Message.h>

#include "swiftfix/scanner.hpp"

namespace swiftfix::shim {

namespace {

std::atomic<bool> g_preparse_enabled{false};

struct AtomicStats {
    std::atomic<std::uint64_t> preparse_attempted{0};
    std::atomic<std::uint64_t> preparse_succeeded{0};
    std::atomic<std::uint64_t> fallback_used{0};
    std::atomic<std::uint64_t> parse_failed{0};
};

AtomicStats& stats() {
    static AtomicStats s;
    return s;
}

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

}  // namespace

void set_preparse_enabled(bool on) noexcept {
    g_preparse_enabled.store(on, std::memory_order_release);
}

bool preparse_enabled() noexcept {
    return g_preparse_enabled.load(std::memory_order_acquire);
}

bool parse_into(std::span<const std::byte> buffer, FIX::Message& out) {
    if (!preparse_enabled()) {
        bool ok = stock_parse(buffer, out);
        if (!ok) stats().parse_failed.fetch_add(1, std::memory_order_relaxed);
        return ok;
    }

    // TODO(phase3): when the scalar scanner is non-stub, attempt preparse
    // and only fall through on non-Ok ScanStatus.
    //
    //     swiftfix::FieldIndex idx;
    //     auto status = swiftfix::default_scanner().scan(buffer, idx);
    //     stats().preparse_attempted.fetch_add(1);
    //     if (status == ScanStatus::Ok) {
    //         if (apply_field_index(idx, buffer, out)) {
    //             stats().preparse_succeeded.fetch_add(1);
    //             return true;
    //         }
    //     }
    //     stats().fallback_used.fetch_add(1);
    //     return stock_parse(buffer, out);

    stats().fallback_used.fetch_add(1, std::memory_order_relaxed);
    bool ok = stock_parse(buffer, out);
    if (!ok) stats().parse_failed.fetch_add(1, std::memory_order_relaxed);
    return ok;
}

ShimStats snapshot_stats() noexcept {
    auto& s = stats();
    return ShimStats{
        s.preparse_attempted.load(std::memory_order_relaxed),
        s.preparse_succeeded.load(std::memory_order_relaxed),
        s.fallback_used.load(std::memory_order_relaxed),
        s.parse_failed.load(std::memory_order_relaxed),
    };
}

void reset_stats() noexcept {
    auto& s = stats();
    s.preparse_attempted.store(0, std::memory_order_relaxed);
    s.preparse_succeeded.store(0, std::memory_order_relaxed);
    s.fallback_used.store(0, std::memory_order_relaxed);
    s.parse_failed.store(0, std::memory_order_relaxed);
}

}  // namespace swiftfix::shim
