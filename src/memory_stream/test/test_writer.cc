// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "memory_stream/memory_stream.h"
#include "testing/writer.h"

#include <fcntl.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <memory>
#include <protobuf_utils/protobuf_utils.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

#include <gtest/gtest.h>

using namespace perf_streams;

class MemoryStreamWriterTest : public perf_streams::testing::StreamWriterTest
{};

TEST_F(MemoryStreamWriterTest, Uncompressed)
{
    output_stream_name = this->test_file_name("ms");
    {
        auto writer =
            std::make_unique<memory_stream::MemoryStreamWriter>(output_stream_name.string().c_str(), 1024, 512);

        memory_stream::Control control;
        control.set_type(memory_stream::Control_Type_START_MEMORY);
        ASSERT_TRUE(writer->write(control));

        memory_stream::Access access;
        access.set_type(memory_stream::Access_Type_READ);
        access.set_source(memory_stream::Access_Source_INSTRUCTION);
        access.set_physical_address(0xdeadbeef);
        access.set_size(8);
        access.set_offset(3);
        ASSERT_TRUE(writer->write(access));

        access.set_type(memory_stream::Access_Type_WRITE);
        access.set_source(memory_stream::Access_Source_DATA);
        access.set_physical_address(0xdeadc0de);
        access.set_size(4);
        access.set_offset(98);
        access.set_originator_id(3);
        access.set_program_counter(0x7fffff8304);
        ASSERT_TRUE(writer->write(access));
    }

    int const fd = open(output_stream_name.string().c_str(), O_RDONLY);
    ASSERT_GT(fd, 2);

    {
        auto input_stream = std::make_unique<google::protobuf::io::FileInputStream>(fd);

        uint32_t magic, ms_version;
        protobuf_utils::read_header_from(input_stream, &magic, &ms_version);
        ASSERT_EQ(magic, 0x534D4250); // 0x50(P) 0x42(B) 0x4D(M) 0x53(S));
        ASSERT_EQ(ms_version, 4);
        ASSERT_TRUE(input_stream->Close());

        memory_stream::MemoryStreamReader msreader(this->test_file_name("ms").string());
        ASSERT_EQ(msreader.filter_dcache_size(), 1024);
        ASSERT_EQ(msreader.filter_icache_size(), 512);
        ASSERT_FALSE(msreader.filter_was_unified());

        memory_stream::Event event;
        ASSERT_TRUE(msreader.read(event));
        ASSERT_TRUE(event.has_control());
        ASSERT_TRUE(memory_stream::Control_Type_START_MEMORY == event.control().type());
        event.Clear();

        memory_stream::Access access1;
        ASSERT_TRUE(msreader.read(access1));
        ASSERT_EQ(memory_stream::Access_Type_READ, access1.type());
        ASSERT_EQ(memory_stream::Access_Source_INSTRUCTION, access1.source());
        ASSERT_EQ(0xdeadbeef, access1.physical_address());
        ASSERT_EQ(8, access1.size());
        ASSERT_EQ(3, access1.offset());
        ASSERT_EQ(0, access1.originator_id());
        ASSERT_EQ(0, access1.program_counter());

        memory_stream::Access access2;
        ASSERT_TRUE(msreader.read(access2));
        ASSERT_EQ(memory_stream::Access_Type_WRITE, access2.type());
        ASSERT_EQ(memory_stream::Access_Source_DATA, access2.source());
        ASSERT_EQ(0xdeadc0de, access2.physical_address());
        ASSERT_EQ(4, access2.size());
        ASSERT_EQ(98, access2.offset());
        ASSERT_EQ(3, access2.originator_id());
        ASSERT_EQ(0x7fffff8304, access2.program_counter());
        ASSERT_FALSE(msreader.read(access2));
    }
}

TEST_F(MemoryStreamWriterTest, Compressed)
{
    output_stream_name = this->test_file_name("ms.xz");
    {
        auto writer =
            std::make_unique<memory_stream::MemoryStreamWriter>(output_stream_name.string().c_str(), 1024, 1024, true);

        memory_stream::Access access;
        access.set_type(memory_stream::Access_Type_READ);
        access.set_physical_address(0xdeadbeef);
        access.set_size(8);
        access.set_offset(3);
        ASSERT_TRUE(writer->write(access));

        access.set_type(memory_stream::Access_Type_WRITE);
        access.set_physical_address(0xdeadc0de);
        access.set_size(4);
        access.set_offset(98);
        access.set_originator_id(3);
        access.set_program_counter(0x7fffff8304);
        ASSERT_TRUE(writer->write(access));
    }
    {
        auto reader = std::make_unique<memory_stream::MemoryStreamReader>(output_stream_name);

        ASSERT_EQ(reader->version(), 4);
        ASSERT_EQ(reader->filter_dcache_size(), 1024);
        ASSERT_EQ(reader->filter_icache_size(), 1024);
        ASSERT_TRUE(reader->filter_was_unified());

        memory_stream::Access access1;
        ASSERT_TRUE(reader->read(access1));
        ASSERT_EQ(memory_stream::Access_Type_READ, access1.type());
        ASSERT_EQ(0xdeadbeef, access1.physical_address());
        ASSERT_EQ(8, access1.size());
        ASSERT_EQ(3, access1.offset());
        ASSERT_EQ(0, access1.originator_id());
        ASSERT_EQ(0, access1.program_counter());

        memory_stream::Access access2;
        ASSERT_TRUE(reader->read(access2));
        ASSERT_EQ(memory_stream::Access_Type_WRITE, access2.type());
        ASSERT_EQ(0xdeadc0de, access2.physical_address());
        ASSERT_EQ(4, access2.size());
        ASSERT_EQ(98, access2.offset());
        ASSERT_EQ(3, access2.originator_id());
        ASSERT_EQ(0x7fffff8304, access2.program_counter());
    }
}
