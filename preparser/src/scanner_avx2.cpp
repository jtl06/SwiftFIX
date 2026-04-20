// scanner_avx2.cpp — AVX2 SIMD implementation.
//
// Shape: scalar headers (tags 8/9/35), then a single AVX2 pass over the
// rest of the message that collects every SOH offset into a stack array,
// then a scalar second pass that parses each field between consecutive
// known SOH boundaries. One bulk SIMD pass amortizes the AVX2 setup cost
// across all fields instead of paying it per field.
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
        if (end - p < 2) [[unlikely]] return ScanStatus::Truncated;
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
        const std::size_t remaining = buffer.size() - body_start;
        if (body_len > remaining) [[unlikely]] return ScanStatus::Truncated;
        const std::size_t body_end = body_start + body_len;
        if (remaining - body_len < 7) [[unlikely]] return ScanStatus::Truncated;

        // tag 35
        if (end - p < 3) [[unlikely]] return ScanStatus::Truncated;
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
        const unsigned char* const scan_end_ptr = body_end_ptr + 7;  // includes "10=NNN<SOH>"

        // pass 1: bulk SOH scan over [p, scan_end_ptr).
        std::uint32_t sohs[kMaxFields];
        std::uint32_t soh_count = 0;
        {
            const __m256i soh_target = _mm256_set1_epi8(0x01);
            const unsigned char* q = p;
            while (scan_end_ptr - q >= 32) {
                const __m256i v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(q));
                std::uint32_t m = static_cast<std::uint32_t>(
                    _mm256_movemask_epi8(_mm256_cmpeq_epi8(v, soh_target)));
                const std::uint32_t base_off = static_cast<std::uint32_t>(q - base);
                while (m) {
                    if (soh_count >= kMaxFields) [[unlikely]] return ScanStatus::TableFull;
                    sohs[soh_count++] = base_off + static_cast<std::uint32_t>(std::countr_zero(m));
                    m &= m - 1;  // clear lowest set bit
                }
                q += 32;
            }
            while (q < scan_end_ptr) {
                if (*q == 0x01) {
                    if (soh_count >= kMaxFields) [[unlikely]] return ScanStatus::TableFull;
                    sohs[soh_count++] = static_cast<std::uint32_t>(q - base);
                }
                ++q;
            }
        }

        // checksum field always contributes one SOH
        if (soh_count == 0) [[unlikely]] return ScanStatus::BadBodyLength;

        // Pass 2: scalar field parse between consecutive SOH boundaries.
        std::uint32_t field_start_off = static_cast<std::uint32_t>(p - base);
        for (std::uint32_t i = 0; i + 1 < soh_count; ++i) {
            if (out.field_count >= kMaxFields) [[unlikely]] return ScanStatus::TableFull;
            const std::uint32_t soh_off = sohs[i];
            const unsigned char* const fs = base + field_start_off;
            const unsigned char* const se = base + soh_off;

            std::uint32_t tag = 0;
            const unsigned char* tp = fs;
            while (tp < se && *tp != '=') {
                const unsigned c = *tp;
                if (!is_digit(c)) [[unlikely]] return ScanStatus::Malformed;
                if (tag > (UINT32_MAX - 9) / 10) [[unlikely]]
                    return ScanStatus::Malformed;
                tag = tag * 10 + (c - '0');
                ++tp;
            }
            if (tp == se) [[unlikely]] return ScanStatus::Malformed;  // no '=' before SOH
            if (tp == fs) [[unlikely]] return ScanStatus::Malformed;  // empty tag
            if (tag == 0) [[unlikely]] return ScanStatus::Malformed;

            if (tag == 95 || tag == 96) [[unlikely]] return ScanStatus::FallbackRequested;

            out.fields[out.field_count++] = FieldEntry{
                field_start_off,
                static_cast<std::uint32_t>(tp - base) + 1,  // past '='
                soh_off,
                tag,
            };
            field_start_off = soh_off + 1;
        }

        // body must end when checksum field starts
        if (field_start_off != static_cast<std::uint32_t>(body_end))
            [[unlikely]] return ScanStatus::BadBodyLength;

        // checksum field: exactly "10=NNN<SOH>" (7 bytes).
        const std::uint32_t cksum_soh_off = sohs[soh_count - 1];
        if (cksum_soh_off != static_cast<std::uint32_t>(body_end) + 6)
            [[unlikely]] return ScanStatus::BadBodyLength;
        if (body_end_ptr[0] != '1' || body_end_ptr[1] != '0' || body_end_ptr[2] != '=')
            [[unlikely]] return ScanStatus::BadBodyLength;
        const unsigned char* const v = body_end_ptr + 3;
        if (((v[0] - '0') > 9u) ||
            ((v[1] - '0') > 9u) ||
            ((v[2] - '0') > 9u)) [[unlikely]]
            return ScanStatus::Malformed;

        if (out.field_count >= kMaxFields) [[unlikely]] return ScanStatus::TableFull;
        out.checksum_idx = static_cast<std::int32_t>(out.field_count);
        out.fields[out.field_count++] = FieldEntry{
            static_cast<std::uint32_t>(body_end),
            static_cast<std::uint32_t>(body_end) + 3,
            cksum_soh_off,
            10,
        };
        out.frame_length = cksum_soh_off + 1;
        return ScanStatus::Ok;
    }

    ScannerKind kind() const noexcept override { return ScannerKind::Avx2; }

  private:
    static constexpr bool is_digit(unsigned c) noexcept { return (c - '0') <= 9u; }
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
