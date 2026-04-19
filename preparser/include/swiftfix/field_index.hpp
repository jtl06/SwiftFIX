// field_index.hpp — data structure handed from the pre-parser to QuickFIX.
//
// The full binary layout, size-class policy, and lifetime rules are described
// in docs/field_index_format.md. This header declares the types; bodies are
// implemented in the scanner translation units.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace swiftfix {

// Byte offsets within the original raw FIX buffer. Using 32-bit offsets packs
// a field entry into 16 bytes and caps single-message size at 4 GiB — well
// beyond anything a real session will produce. The '=' byte is not stored
// because the consumer only needs [value_start, value_end) to copy the value
// and tag_number to dispatch; see docs/field_index_format.md.
struct FieldEntry {
    std::uint32_t tag_start;     // offset of the first tag-digit byte
    std::uint32_t value_start;   // offset of the first value byte (byte after '=')
    std::uint32_t value_end;     // offset of the <SOH> terminating the value
    std::uint32_t tag_number;    // parsed tag number (e.g. 35 for MsgType)
};

// Compile-time cap on header/body fields we index inline. Messages exceeding
// this emit ScanStatus::TableFull and the caller falls back to stock parsing.
// At 16 B per entry, 256 entries plus the 28 B FieldIndex header occupy
// 4124 B — one page plus a cache line. Tuned against typical FIX.4.4 traffic
// where per-message field counts are well under 50.
inline constexpr std::size_t kMaxFields = 256;

struct FieldIndex {
    // Header fast-access slots. -1 means "not found / not yet scanned".
    std::int32_t begin_string_idx = -1;   // tag 8  — BeginString
    std::int32_t body_length_idx  = -1;   // tag 9  — BodyLength
    std::int32_t msg_type_idx     = -1;   // tag 35 — MsgType
    std::int32_t checksum_idx     = -1;   // tag 10 — CheckSum

    // Parsed body length (tag 9 value). 0 until the header is scanned.
    std::uint32_t declared_body_length = 0;

    // Total number of bytes consumed by this message, including trailing <SOH>.
    std::uint32_t frame_length = 0;

    // Number of populated entries in `fields`.
    std::uint32_t field_count = 0;

    std::array<FieldEntry, kMaxFields> fields{};

    void reset() noexcept {
        begin_string_idx = body_length_idx = msg_type_idx = checksum_idx = -1;
        declared_body_length = 0;
        frame_length = 0;
        field_count = 0;
    }
};

static_assert(sizeof(FieldEntry) == 16, "FieldEntry layout must remain stable");

}  // namespace swiftfix
