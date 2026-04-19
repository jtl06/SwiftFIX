// scanner_scalar.cpp — reference scalar implementation.
//
// Scalar implementation completed.
#include "swiftfix/scanner.hpp"

namespace swiftfix {

namespace {

class ScalarScanner final : public Scanner {
  public:

    ScanStatus scan(std::span<const std::byte> buffer, FieldIndex& out) noexcept override {
        //reset buffer
        out.reset();

        //check if empty 
        if (buffer.empty()) return ScanStatus::Truncated;

        //init
        std::size_t i = 0;
        FieldEntry entry;

        // first entry - begin string
        switch (scan_one_field(buffer, i, entry)) {
            case FieldScan::Ok:        break;
            case FieldScan::Truncated: return ScanStatus::Truncated;
            case FieldScan::Malformed: return ScanStatus::Malformed;
        }
        //check if 8
        if (entry.tag_number != 8) return ScanStatus::BadHeader;
        
        //set fast access slot
        out.begin_string_idx = static_cast<std::int32_t>(out.field_count);
        out.fields[out.field_count++] = entry;

        
        //second entry - body length
        switch (scan_one_field(buffer, i, entry)) {
            case FieldScan::Ok:        break;
            case FieldScan::Truncated: return ScanStatus::Truncated;
            case FieldScan::Malformed: return ScanStatus::Malformed;
        }
        //check if 9
        if (entry.tag_number != 9) return ScanStatus::BadHeader;
        //set fast access slot
        out.body_length_idx = static_cast<std::int32_t>(out.field_count);
        //set body length
        std::uint32_t body_len = 0;
        if (!parse_uint(buffer, entry.value_start, entry.value_end, body_len))
            return ScanStatus::Malformed;
        out.declared_body_length = body_len;
        out.fields[out.field_count++] = entry;

        // check if buffer fits 
        const std::size_t body_start = i;
        const std::size_t body_end   = body_start + body_len;
        if (body_end + 7 > buffer.size()) return ScanStatus::Truncated;

        //third entry - should be 35
        switch (scan_one_field(buffer, i, entry)) {
            case FieldScan::Ok:        break;
            case FieldScan::Truncated: return ScanStatus::Truncated;
            case FieldScan::Malformed: return ScanStatus::Malformed;
        }
        //check 35
        if (entry.tag_number != 35) return ScanStatus::BadHeader;
        //set fast access slot
        out.msg_type_idx = static_cast<std::int32_t>(out.field_count);
        out.fields[out.field_count++] = entry;

        // main body loop
        while (i < body_end) {
            if (out.field_count >= kMaxFields) return ScanStatus::TableFull;
            switch (scan_one_field(buffer, i, entry)) {
                case FieldScan::Ok:        break;
                case FieldScan::Truncated: return ScanStatus::Truncated;
                case FieldScan::Malformed: return ScanStatus::Malformed;
            }
            if (entry.tag_number == 95 || entry.tag_number == 96)
                return ScanStatus::FallbackRequested;
            out.fields[out.field_count++] = entry;
        }
        // check overshoot
        if (i != body_end) return ScanStatus::BadBodyLength;

        // check ending and checksum
        switch (scan_one_field(buffer, i, entry)) {
            case FieldScan::Ok:        break;
            case FieldScan::Truncated: return ScanStatus::Truncated;
            case FieldScan::Malformed: return ScanStatus::Malformed;
        }
        //check 10
        if (entry.tag_number != 10) return ScanStatus::Malformed;
        if (entry.value_end - entry.value_start != 3) return ScanStatus::Malformed;
        // check 3 bytes are digits. checksum handled by quickfix
        [[maybe_unused]] std::uint32_t cksum_value = 0;
        if (!parse_uint(buffer, entry.value_start, entry.value_end, cksum_value))
            return ScanStatus::Malformed;

        //set fast access slot
        out.checksum_idx = static_cast<std::int32_t>(out.field_count);
        out.fields[out.field_count++] = entry;
        out.frame_length = static_cast<std::uint32_t>(i);
        return ScanStatus::Ok;
    }

    ScannerKind kind() const noexcept override { return ScannerKind::Scalar; }

  private:
    enum class FieldScan { Ok, Truncated, Malformed };

    // Scans one "tag=value<SOH>" triple starting at buffer[cursor]. On Ok,
    // fills `entry` and advances `cursor` to the byte just past the
    // terminating <SOH>. On Truncated, the buffer ended mid-field with a
    // prefix that could still complete. On Malformed, the bytes seen cannot
    // form a valid field regardless of continuation.
    static FieldScan scan_one_field(std::span<const std::byte> buffer,
                                    std::size_t& cursor,
                                    FieldEntry& entry) noexcept {
        const std::size_t tag_start = cursor;
        std::uint32_t tag = 0;

        while (cursor < buffer.size()) {
            const unsigned c = std::to_integer<unsigned char>(buffer[cursor]);
            if (is_digit(c)) {
                // Guard against accumulator wrap on a pathological 10+ digit tag.
                // (UINT32_MAX - 9) / 10 is the largest `tag` value that can still
                // accept any next digit without overflowing uint32.
                // if (tag > (UINT32_MAX - 9) / 10) return FieldScan::Malformed;
                tag = tag * 10 + (c - '0');
                ++cursor;
            } else if (c == '=') {
                break;
            } else {
                return FieldScan::Malformed;
            }
        }
        if (cursor == buffer.size()) return FieldScan::Truncated;

        const std::size_t eq_pos = cursor;
        if (tag_start == eq_pos) return FieldScan::Malformed;  // empty tag: "=value"
        if (tag == 0)            return FieldScan::Malformed;  // FIX reserves tag 0
        ++cursor;                                              // past '='
        const std::size_t value_start = cursor;

        while (cursor < buffer.size()) {
            const unsigned c = std::to_integer<unsigned char>(buffer[cursor]);
            if (c == 0x01) break;
            ++cursor;
        }
        if (cursor == buffer.size()) return FieldScan::Truncated;

        const std::size_t soh_pos = cursor;
        ++cursor;  // past <SOH>

        entry = FieldEntry{
            static_cast<std::uint32_t>(tag_start),
            static_cast<std::uint32_t>(value_start),
            static_cast<std::uint32_t>(soh_pos),
            tag,
        };
        return FieldScan::Ok;
    }

    // Parses buffer[from, to) as a base-10 uint32. Returns false on a
    // non-digit byte, an empty range, or overflow. Used for tag 9 (body
    // length) in stage 2 and will be reused for the 3-digit checksum in
    // stage 5.
    static bool parse_uint(std::span<const std::byte> buffer,
                           std::size_t from, std::size_t to,
                           std::uint32_t& out) noexcept {
        if (from == to) return false;
        std::uint32_t v = 0;
        for (std::size_t k = from; k < to; ++k) {
            const unsigned c = std::to_integer<unsigned char>(buffer[k]);
            if (!is_digit(c)) return false;
            if (v > (UINT32_MAX - 9) / 10) return false;   // overflow guard
            v = v * 10 + (c - '0');
        }
        out = v;
        return true;
    }

    //returns digit if between 0 and 9
    static constexpr bool is_digit(unsigned c) noexcept { return (c - '0') <= 9u; }
};

}  // namespace

Scanner& scalar_scanner_instance() noexcept {
    static ScalarScanner instance;
    return instance;
}

}  // namespace swiftfix
