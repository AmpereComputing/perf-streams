/*
 * Copyright (c) 2026, Ampere Computing LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <filesystem>

#include <gtest/gtest.h>

namespace perf_streams::testing {

using std::filesystem::path;

class StreamWriterTest : public ::testing::Test
{
protected:
    path test_file_name(const char* extension)
    {
        const ::testing::TestInfo* const test_info = ::testing::UnitTest::GetInstance()->current_test_info();
        path p(test_info->name());
        p.replace_extension(extension);
        return p;
    }

    path output_stream_name;
    google::protobuf::io::FileOutputStream* output_stream;

    void TearDown() override
    {
        if (!output_stream_name.string().empty()) {
            ASSERT_EQ(0, unlink(output_stream_name.string().c_str()));
        }
    }
};

} // namespace perf_streams::testing
