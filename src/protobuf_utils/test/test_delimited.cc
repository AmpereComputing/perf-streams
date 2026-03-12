// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "protobuf_utils/protobuf_utils.h"
#include "protobuf_utils/test/example.pb.h"

#include <fcntl.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>

#include <gtest/gtest.h>

using namespace std;
using namespace perf_streams::protobuf_utils;

class DelimitedTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        int fd = open("example.es", O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
        ASSERT_GT(fd, 2);
        output_stream = new google::protobuf::io::FileOutputStream(fd);
    }
    void TearDown() override
    {
        ASSERT_EQ(0, unlink("example.es"));
        delete output_stream;
    }
    google::protobuf::io::FileOutputStream* output_stream;
};

TEST_F(DelimitedTest, Write)
{
    example::Hello hello;
    hello.set_id(1);
    hello.set_message("world");
    ASSERT_TRUE(write_delimited_to(hello, output_stream));
    ASSERT_TRUE(output_stream->Close());
}

TEST_F(DelimitedTest, Read)
{
    example::Hello hello;
    hello.set_id(1);
    hello.set_message("hello");
    ASSERT_TRUE(write_delimited_to(hello, output_stream));
    hello.set_id(2);
    hello.set_message("world");
    ASSERT_TRUE(write_delimited_to(hello, output_stream));
    ASSERT_TRUE(output_stream->Close());

    int fd = open("example.es", O_RDONLY);
    google::protobuf::io::FileInputStream* input_stream;
    input_stream = new google::protobuf::io::FileInputStream(fd);

    example::Hello read_hello;
    ASSERT_TRUE(read_delimited_from(input_stream, &read_hello, nullptr));
    ASSERT_EQ("hello", read_hello.message());

    ASSERT_TRUE(read_delimited_from(input_stream, &read_hello, nullptr));
    ASSERT_EQ("world", read_hello.message());
    delete input_stream;
}
