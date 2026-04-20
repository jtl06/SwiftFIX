// scanner_avx2.cpp — AVX2 SIMD implementation.
//
// Shape: scalar tag parse + scalar header specialization (tags 8/9/35),
// AVX2 only on the value-SOH scan (the wide span). The eq-byte is at most
// a few bytes after the tag start, so SIMD doesn't pay there.
// Runtime dispatch gates entry via avx2_is_available_at_runtime().
#include "swiftfix/scanner.hpp"

#include <immintrin.h>
#include <bit>

namespace swiftfix {

namespace {

class Avx2Scanner final : public Scanner {
  public:

    ScanStatus scan(std::span<const std::byte> buffer, FieldIndex& out) noexcept override {
        out.reset();

        // header check
        if (buffer.size() < 12) [[unlikely]] return ScanStatus::Truncated;

        const unsigned char* const base = reinterpret_cast<const unsigned char*>(buffer.data());
        const unsigned char* const end  = base + buffer.size();
        const unsigned char*       p    = base;

        // tag 8
        if (p[0] != '8' || p[1] != '=') [[unlikely]] return ScanStatus::BadHeader;
        const unsigned char* const t8_start = p;
        p += 2;
        const unsigned char* const v8_start = p;
        while (p < end && *p != 0x01) ++p;
        if (p == end) [[unlikely]] return ScanStatus::Truncated;
        out.begin_string_idx = static_cast<std::int32_t>(out.field_count);
        out.fields[out.field_count++] = FieldEntry{
            static_cast<std::uint32_t>(t8_start - base),
            static_cast<std::uint32_t>(v8_start - base),
            static_cast<std::uint32_t>(p - base),
            8,
        };
        ++p;  // past SOH

        // tag 9
        if (p[0] != '9' || p[1] != '=') [[unlikely]] return ScanStatus::BadHeader;
        const unsigned char* const t9_start = p;
        p += 2;
        const unsigned char* const v9_start = p;
        std::uint32_t body_len = 0;
        while (p < end && *p != 0x01) {
            if (!is_digit(*p))                    [[unlikely]] return ScanStatus::Malformed;
            if (body_len > (UINT32_MAX - 9) / 10) [[unlikely]] return ScanStatus::Malformed;
            body_len = body_len * 10 + (*p - '0');
            ++p;
        }
        if (p == end)      [[unlikely]] return ScanStatus::Truncated;
        if (p == v9_start) [[unlikely]] return ScanStatus::Malformed;  // empty body length
        out.body_length_idx = static_cast<std::int32_t>(out.field_count);
        out.declared_body_length = body_len;
        out.fields[out.field_count++] = FieldEntry{
            static_cast<std::uint32_t>(t9_start - base),
            static_cast<std::uint32_t>(v9_start - base),
            static_cast<std::uint32_t>(p - base),
            9,
        };
        ++p;  // past SOH

        // body bounds check
        const std::size_t body_start = static_cast<std::size_t>(p - base);
        const std::size_t body_end   = body_start + body_len;
        if (body_end + 7 > buffer.size()) [[unlikely]] return ScanStatus::Truncated;

        // tag 35
        if (p[0] != '3' || p[1] != '5' || p[2] != '=') [[unlikely]] return ScanStatus::BadHeader;
        const unsigned char* const t35_start = p;
        p += 3;
        const unsigned char* const v35_start = p;
        while (p < end && *p != 0x01) ++p;
        if (p == end) [[unlikely]] return ScanStatus::Truncated;
        out.msg_type_idx = static_cast<std::int32_t>(out.field_count);
        out.fields[out.field_count++] = FieldEntry{
            static_cast<std::uint32_t>(t35_start - base),
            static_cast<std::uint32_t>(v35_start - base),
            static_cast<std::uint32_t>(p         - base),
            35,
        };
        ++p;  // past SOH

        const unsigned char* const body_end_ptr = base + body_end;
        FieldEntry entry;

        // main body loop
        while (p < body_end_ptr) {
            if (out.field_count >= kMaxFields) [[unlikely]] return ScanStatus::TableFull;
            switch (scan_one_field(p, end, base, entry)) {
                case FieldScan::Ok:        break;
                [[unlikely]] case FieldScan::Truncated: return ScanStatus::Truncated;
                [[unlikely]] case FieldScan::Malformed: return ScanStatus::Malformed;
            }
            if (entry.tag_number == 95 || entry.tag_number == 96) [[unlikely]]
                return ScanStatus::FallbackRequested;
            out.fields[out.field_count++] = entry;
        }
        // check overshoot
        if (p != body_end_ptr) [[unlikely]] return ScanStatus::BadBodyLength;

        // check ending and checksum
        switch (scan_one_field(p, end, base, entry)) {
            case FieldScan::Ok:        break;
            [[unlikely]] case FieldScan::Truncated: return ScanStatus::Truncated;
            [[unlikely]] case FieldScan::Malformed: return ScanStatus::Malformed;
        }
        //check if 10
        if (entry.tag_number != 10) [[unlikely]] return ScanStatus::BadBodyLength;
        if (entry.value_end - entry.value_start != 3) [[unlikely]] return ScanStatus::Malformed;
        // FIX checksum is exactly 3 ASCII digits; value math handled by quickfix.
        const unsigned char* const v = base + entry.value_start;
        if (((v[0] - '0') > 9u) ||
            ((v[1] - '0') > 9u) ||
            ((v[2] - '0') > 9u)) [[unlikely]]
            return ScanStatus::Malformed;

        //set fast access slot
        out.checksum_idx = static_cast<std::int32_t>(out.field_count);
        out.fields[out.field_count++] = entry;
        out.frame_length = static_cast<std::uint32_t>(p - base);
        return ScanStatus::Ok;
    }

    ScannerKind kind() const noexcept override { return ScannerKind::Avx2; }

  private:
    enum class FieldScan { Ok, Truncated, Malformed };

    // Scalar tag parse + AVX2 value-SOH scan. Tags are short (1–4 bytes
    // typical), so SIMD on the eq-search isn't worth the bookkeeping.
    static FieldScan scan_one_field(const unsigned char*& p,
                                    const unsigned char* end,
                                    const unsigned char* base,
                                    FieldEntry& entry) noexcept {
        const unsigned char* const tag_start = p;

        std::uint32_t tag = 0;
        while (p < end) {
            const unsigned c = *p;
            if (is_digit(c)) {
                tag = tag * 10 + (c - '0');
                p++;
            } else if (c == '=') {
                break;
            } else [[unlikely]] {
                return FieldScan::Malformed;
            }
        }
        if (p == end)       [[unlikely]] return FieldScan::Truncated;
        if (p == tag_start) [[unlikely]] return FieldScan::Malformed;  // empty tag: "=value"
        if (tag == 0)       [[unlikely]] return FieldScan::Malformed;  // FIX reserves tag 0

        p++;  // past '='
        const unsigned char* const value_start = p;

        const unsigned char* const soh_pos = find_soh_avx2(p, end);
        if (!soh_pos) [[unlikely]] return FieldScan::Truncated;
        p = soh_pos + 1;  // past <SOH>

        entry = FieldEntry{
            static_cast<std::uint32_t>(tag_start - base),
            static_cast<std::uint32_t>(value_start - base),
            static_cast<std::uint32_t>(soh_pos - base),
            tag,
        };
        return FieldScan::Ok;
    }

    static constexpr bool is_digit(unsigned c) noexcept { return (c - '0') <= 9u; }

    // 32-byte AVX2 SOH scan with a scalar tail.
    static const unsigned char* find_soh_avx2(const unsigned char* p,
                                              const unsigned char* end) noexcept {
        const __m256i target = _mm256_set1_epi8(0x01);

        while (static_cast<std::size_t>(end - p) >= 32) {
            const __m256i vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p));
            const __m256i res = _mm256_cmpeq_epi8(vec, target);
            const std::uint32_t mask = static_cast<std::uint32_t>(_mm256_movemask_epi8(res));
            if (mask) return p + std::countr_zero(mask);
            p += 32;
        }

        while (p < end) {
            if (*p == 0x01) return p;
            ++p;
        }
        return nullptr;
    }
};

}  // namespace

bool avx2_is_available_at_runtime() noexcept {
    return __builtin_cpu_supports("avx2");
}

Scanner* avx2_scanner_instance() noexcept {
    static Avx2Scanner instance;
    return &instance;
}

}  // namespace swiftfix
