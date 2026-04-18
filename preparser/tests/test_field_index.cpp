// test_field_index.cpp — FieldIndex layout and reset behavior.
#include <gtest/gtest.h>

#include "swiftfix/field_index.hpp"

TEST(FieldIndex, DefaultConstructedIsEmpty) {
    swiftfix::FieldIndex idx;
    EXPECT_EQ(idx.begin_string_idx, -1);
    EXPECT_EQ(idx.body_length_idx,  -1);
    EXPECT_EQ(idx.msg_type_idx,     -1);
    EXPECT_EQ(idx.checksum_idx,     -1);
    EXPECT_EQ(idx.declared_body_length, 0u);
    EXPECT_EQ(idx.frame_length, 0u);
    EXPECT_EQ(idx.field_count, 0u);
}

TEST(FieldIndex, ResetClearsAllScalars) {
    swiftfix::FieldIndex idx;
    idx.begin_string_idx = 0;
    idx.msg_type_idx = 2;
    idx.declared_body_length = 148;
    idx.frame_length = 165;
    idx.field_count = 17;

    idx.reset();

    EXPECT_EQ(idx.begin_string_idx, -1);
    EXPECT_EQ(idx.msg_type_idx,     -1);
    EXPECT_EQ(idx.declared_body_length, 0u);
    EXPECT_EQ(idx.frame_length, 0u);
    EXPECT_EQ(idx.field_count, 0u);
}

TEST(FieldIndex, EntrySizeIsStable) {
    // The scanner hot path assumes this layout; regressions must trip CI.
    static_assert(sizeof(swiftfix::FieldEntry) == 20);
    SUCCEED();
}
