// test_scanner_corpus.cpp — scanner coverage over checked-in FIX corpora.
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "swiftfix/scanner.hpp"
#include "swiftfix/status.hpp"

namespace swiftfix {
bool avx2_is_available_at_runtime() noexcept;
}  // namespace swiftfix

namespace {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

std::span<const std::byte> as_bytes(const std::string& s) {
    return {reinterpret_cast<const std::byte*>(s.data()), s.size()};
}

void expect_same_index(const swiftfix::FieldIndex& a,
                       const swiftfix::FieldIndex& b) {
    EXPECT_EQ(a.begin_string_idx, b.begin_string_idx);
    EXPECT_EQ(a.body_length_idx, b.body_length_idx);
    EXPECT_EQ(a.msg_type_idx, b.msg_type_idx);
    EXPECT_EQ(a.checksum_idx, b.checksum_idx);
    EXPECT_EQ(a.declared_body_length, b.declared_body_length);
    EXPECT_EQ(a.frame_length, b.frame_length);
    ASSERT_EQ(a.field_count, b.field_count);
    for (std::uint32_t i = 0; i < a.field_count; ++i) {
        EXPECT_EQ(a.fields[i].tag_start, b.fields[i].tag_start);
        EXPECT_EQ(a.fields[i].value_start, b.fields[i].value_start);
        EXPECT_EQ(a.fields[i].value_end, b.fields[i].value_end);
        EXPECT_EQ(a.fields[i].tag_number, b.fields[i].tag_number);
    }
}

void expect_avx2_matches_scalar(std::span<const std::byte> bytes) {
    auto* scalar = swiftfix::scanner_for(swiftfix::ScannerKind::Scalar);
    ASSERT_NE(scalar, nullptr);

    swiftfix::FieldIndex scalar_idx;
    const auto scalar_status = scalar->scan(bytes, scalar_idx);
    ASSERT_EQ(scalar_status, swiftfix::ScanStatus::Ok);

    auto* avx2 = swiftfix::scanner_for(swiftfix::ScannerKind::Avx2);
    if (!swiftfix::avx2_is_available_at_runtime()) {
        ASSERT_EQ(avx2, nullptr);
        return;
    }
    ASSERT_NE(avx2, nullptr);

    swiftfix::FieldIndex avx2_idx;
    const auto avx2_status = avx2->scan(bytes, avx2_idx);
    ASSERT_EQ(avx2_status, scalar_status);
    expect_same_index(scalar_idx, avx2_idx);
}

}  // namespace

TEST(ScannerCorpus, ValidFilesScanAndAvx2MatchesScalar) {
    for (const auto& entry :
         std::filesystem::directory_iterator(SWIFTFIX_CORPUS_VALID_DIR)) {
        if (!entry.is_regular_file()) continue;
        SCOPED_TRACE(entry.path().string());
        const std::string bytes = read_file(entry.path());
        expect_avx2_matches_scalar(as_bytes(bytes));
    }
}

TEST(ScannerCorpus, BulkStreamScansFrameByFrame) {
    const std::string stream = read_file(SWIFTFIX_BULK_STREAM);
    auto* scalar = swiftfix::scanner_for(swiftfix::ScannerKind::Scalar);
    ASSERT_NE(scalar, nullptr);

    auto* avx2 = swiftfix::scanner_for(swiftfix::ScannerKind::Avx2);
    const bool check_avx2 = swiftfix::avx2_is_available_at_runtime();
    if (check_avx2) ASSERT_NE(avx2, nullptr);

    std::size_t pos = 0;
    std::size_t frames = 0;
    while (pos < stream.size()) {
        const auto* p = reinterpret_cast<const std::byte*>(stream.data() + pos);
        const std::span<const std::byte> bytes(p, stream.size() - pos);

        swiftfix::FieldIndex scalar_idx;
        ASSERT_EQ(scalar->scan(bytes, scalar_idx), swiftfix::ScanStatus::Ok);
        ASSERT_GT(scalar_idx.frame_length, 0u);

        if (check_avx2) {
            swiftfix::FieldIndex avx2_idx;
            ASSERT_EQ(avx2->scan(bytes, avx2_idx), swiftfix::ScanStatus::Ok);
            expect_same_index(scalar_idx, avx2_idx);
        }

        pos += scalar_idx.frame_length;
        ++frames;
    }
    EXPECT_GT(frames, 0u);
}
