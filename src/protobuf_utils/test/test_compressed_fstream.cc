// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "protobuf_utils/compressed_fstream.h"
#include "protobuf_utils/protobuf_stream.h"
#include "protobuf_utils/protobuf_utils.h"
#include "protobuf_utils/test/example.pb.h"
#include "testing/capture.h"

#include <filesystem>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <ostream>
#include <sstream>

#include <gtest/gtest.h>

using namespace std::filesystem;
using namespace perf_streams::protobuf_utils;

class CompressedFStreamTest : public ::testing::Test
{
protected:
    std::filesystem::path test_file_name(const std::string& extension)
    {
        const ::testing::TestInfo* const test_info = ::testing::UnitTest::GetInstance()->current_test_info();
        path p(test_info->name());
        p.replace_extension(extension);
        return p;
    }
};

TEST_F(CompressedFStreamTest, CompressionFromFilename)
{
    ASSERT_EQ(CompressionType::NONE, compression_from_filename("traces.is"));
    ASSERT_EQ(CompressionType::NONE, compression_from_filename("really_long.log"));
    ASSERT_EQ(CompressionType::ZLIB, compression_from_filename("http.logs.zlib"));
    ASSERT_EQ(CompressionType::ZLIB, compression_from_filename("http.logs.zl"));
    ASSERT_EQ(CompressionType::BZ2, compression_from_filename("package.tar.bz2"));
    ASSERT_EQ(CompressionType::GZIP, compression_from_filename("boring.log.gz"));
    ASSERT_EQ(CompressionType::XZ, compression_from_filename("boring.log.xz"));
}

class CompressedFStreamIOTest : public CompressedFStreamTest, public ::testing::WithParamInterface<std::string>
{
public:
    std::string test_string;
    std::string suffix;
    std::filesystem::path test_path;

    CompressedFStreamIOTest()
        : test_string((GetParam().empty() ? "un" : GetParam() + "_") + "compressed_testing_123_testing"),
          suffix(GetParam().empty() ? "log" : "log." + GetParam()),
          test_path((GetParam().empty() ? "un" : GetParam()) + "compressed." + suffix)
    {
    }

    void TearDown() override { std::filesystem::remove(test_path); }
};

TEST(StreamIOTest, Stdout)
{
    auto capture = perf_streams::testing::CaptureStdout();
    auto out = open_compressed_ostream("-");
    *out << "test" << '\n';
    out.reset();
    EXPECT_EQ(*capture, "test\n");
}

TEST_P(CompressedFStreamIOTest, ReadWrite)
{
    auto out = open_compressed_ostream(test_path.string().c_str());
    *out << test_string << '\n';
    out.reset();
    ASSERT_TRUE(std::filesystem::exists(test_path));

    auto in = open_compressed_istream(test_path.string().c_str());
    std::string line;
    *in >> line;
    EXPECT_EQ(line, test_string);
}

INSTANTIATE_TEST_SUITE_P(CompressedFStreamIOTest,
                         CompressedFStreamIOTest,
                         ::testing::Values("", "zlib", "gz", "bz2", "xz"));

class CompressedProtobufStreamSecurityTest : public ::testing::Test, public ::testing::WithParamInterface<std::string>
{
protected:
    std::filesystem::path stream_path;
    std::filesystem::path marker_path;

    void SetUp() override
    {
        const auto extension = GetParam();
        marker_path = "protobuf_stream_shell_marker_" + extension;
        stream_path = "protobuf_stream; touch " + marker_path.string() + ";.es." + extension;
        std::filesystem::remove(stream_path);
        std::filesystem::remove(marker_path);
    }

    void TearDown() override
    {
        std::filesystem::remove(stream_path);
        std::filesystem::remove(marker_path);
    }
};

TEST_P(CompressedProtobufStreamSecurityTest, ReaderTreatsMetacharactersAsPath)
{
    constexpr uint32_t magic = 0x12345678;
    constexpr uint32_t version = 4;

    {
        auto out = open_compressed_ostream(stream_path.string().c_str());
        google::protobuf::io::OstreamOutputStream raw_output(out.get());
        example::Hello hello;
        hello.set_id(42);
        hello.set_message("Hello, compressed path!");

        ASSERT_TRUE(write_header_to(magic, version, &raw_output));
        ASSERT_TRUE(write_delimited_to(hello, &raw_output));
    }

    ProtobufStreamReader reader(stream_path, magic, version);
    example::Hello read_hello;
    ASSERT_TRUE(reader.read(read_hello));
    EXPECT_EQ(read_hello.id(), 42);
    EXPECT_EQ(read_hello.message(), "Hello, compressed path!");
    EXPECT_FALSE(std::filesystem::exists(marker_path));
}

INSTANTIATE_TEST_SUITE_P(CompressedProtobufStreamSecurityTest,
                         CompressedProtobufStreamSecurityTest,
                         ::testing::Values("gz", "xz"));
