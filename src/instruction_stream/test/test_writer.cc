// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "instruction_stream/instruction_stream.h"
#include "testing/writer.h"

#include <fcntl.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <protobuf_utils/protobuf_utils.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <gtest/gtest.h>

using namespace perf_streams;

class InstructionStreamWriterTest : public perf_streams::testing::StreamWriterTest
{
protected:
    void write_control(instruction_stream::InstructionStreamWriter* writer, instruction_stream::Control_Type type)
    {
        instruction_stream::Event event;
        instruction_stream::Control control;
        control.set_type(type);
        event.mutable_control()->CopyFrom(control);
        writer->write(event);
    }
};

TEST_F(InstructionStreamWriterTest, Uncompressed)
{
    instruction_stream::Features features;
    features.set_divide_sqrt_registers(false);
    features.set_cache_maintenance_registers(true);
    features.set_memory_translation(true);
    features.set_memory_values(false);

    auto* writer =
        new instruction_stream::InstructionStreamWriter(this->test_file_name("is").string().c_str(), features);

    instruction_stream::Instruction instruction;
    instruction.set_opcode(0xffffffff);
    ASSERT_TRUE(writer->write(instruction));

    write_control(writer, instruction_stream::Control_Type_START_MEASUREMENT);

    instruction.set_opcode(0xeeeeeeee);
    instruction.add_memop()->set_virtual_address(0xaaaaaaaaaaaaaaaa);
    ASSERT_TRUE(writer->write(instruction));

    write_control(writer, instruction_stream::Control_Type_STOP_MEASUREMENT);
    writer->finalize();

    delete writer;

    int fd = open(this->test_file_name("is").string().c_str(), O_RDONLY);
    ASSERT_GT(fd, 2);

    google::protobuf::io::FileInputStream* input_stream;
    input_stream = new google::protobuf::io::FileInputStream(fd);

    uint32_t magic, is_version;
    protobuf_utils::read_header_from(input_stream, &magic, &is_version);
    ASSERT_EQ(magic, 0x53494250); // 0x50(P) 0x42(B) 0x49(I) 0x53(S));
    ASSERT_EQ(is_version, 6);

    // are the feature bits preserved?
    instruction_stream::Features features1;
    ASSERT_TRUE(protobuf_utils::read_delimited_from(input_stream, &features1, nullptr));
    ASSERT_FALSE(features1.divide_sqrt_registers());
    ASSERT_TRUE(features1.cache_maintenance_registers());
    ASSERT_TRUE(features1.memory_translation());
    ASSERT_FALSE(features1.memory_values());

    instruction_stream::Event event1;
    ASSERT_TRUE(protobuf_utils::read_delimited_from(input_stream, &event1, nullptr));
    ASSERT_TRUE(event1.has_instruction());
    ASSERT_EQ(0xffffffff, event1.instruction().opcode());
    ASSERT_EQ(0, event1.instruction().memop_size());

    instruction_stream::Event event2;
    ASSERT_TRUE(protobuf_utils::read_delimited_from(input_stream, &event2, nullptr));
    ASSERT_TRUE(event2.has_control());
    ASSERT_EQ(event2.control().type(), instruction_stream::Control_Type_START_MEASUREMENT);

    instruction_stream::Event event3;
    ASSERT_TRUE(protobuf_utils::read_delimited_from(input_stream, &event3, nullptr));
    ASSERT_TRUE(event3.has_instruction());
    ASSERT_EQ(0xeeeeeeee, event3.instruction().opcode());
    ASSERT_EQ(1, event3.instruction().memop_size());
    ASSERT_EQ(0xaaaaaaaaaaaaaaaa, event3.instruction().memop(0).virtual_address());

    instruction_stream::Event event4;
    ASSERT_TRUE(protobuf_utils::read_delimited_from(input_stream, &event4, nullptr));
    ASSERT_TRUE(event4.has_control());
    ASSERT_EQ(event4.control().type(), instruction_stream::Control_Type_STOP_MEASUREMENT);

    ASSERT_TRUE(input_stream->Close());

    ASSERT_EQ(0, unlink(this->test_file_name("is").string().c_str()));

    delete input_stream;
}

TEST_F(InstructionStreamWriterTest, Compressed)
{
    instruction_stream::Features features;
    features.set_divide_sqrt_registers(false);
    features.set_cache_maintenance_registers(true);
    features.set_memory_translation(true);
    features.set_memory_values(false);

    auto* writer = new instruction_stream::InstructionStreamWriter(test_file_name("is.xz"), features);

    write_control(writer, instruction_stream::Control_Type_START_MEASUREMENT);

    instruction_stream::Instruction instruction;
    instruction.set_opcode(0xffffffff);
    ASSERT_TRUE(writer->write(instruction));

    write_control(writer, instruction_stream::Control_Type_STOP_MEASUREMENT);

    instruction.set_opcode(0xeeeeeeee);
    instruction.add_memop()->set_virtual_address(0xaaaaaaaaaaaaaaaa);
    ASSERT_TRUE(writer->write(instruction));
    writer->finalize();
    delete writer;

    auto* reader = new instruction_stream::InstructionStreamReader(test_file_name("is.xz"));

    // are the feature bits preserved?
    ASSERT_FALSE(reader->features().divide_sqrt_registers());
    ASSERT_TRUE(reader->features().cache_maintenance_registers());
    ASSERT_TRUE(reader->features().memory_translation());
    ASSERT_FALSE(reader->features().memory_values());

    ASSERT_EQ(reader->version(), 6);

    instruction_stream::Event event1;
    ASSERT_TRUE(reader->read(event1));
    ASSERT_TRUE(event1.has_control());
    ASSERT_EQ(event1.control().type(), instruction_stream::Control_Type_START_MEASUREMENT);

    instruction_stream::Instruction instruction2;
    ASSERT_TRUE(reader->read(instruction2));
    ASSERT_EQ(0xffffffff, instruction2.opcode());
    ASSERT_EQ(0, instruction2.memop_size());

    instruction_stream::Event event3;
    ASSERT_TRUE(reader->read(event3));
    ASSERT_TRUE(event3.has_control());
    ASSERT_EQ(event3.control().type(), instruction_stream::Control_Type_STOP_MEASUREMENT);

    instruction_stream::Instruction instruction4;
    ASSERT_TRUE(reader->read(instruction4));
    ASSERT_EQ(0xeeeeeeee, instruction4.opcode());
    ASSERT_EQ(1, instruction4.memop_size());
    ASSERT_EQ(0xaaaaaaaaaaaaaaaa, instruction4.memop(0).virtual_address());

    delete reader;

    ASSERT_EQ(0, unlink(this->test_file_name("is.xz").string().c_str()));
}

TEST_F(InstructionStreamWriterTest, testInvalidControlOrdering)
{
    instruction_stream::Features features;
    auto* writer =
        new instruction_stream::InstructionStreamWriter(this->test_file_name("is").string().c_str(), features);
    EXPECT_THROW(writer->finalize(), std::runtime_error);
    EXPECT_THROW(write_control(writer, instruction_stream::Control_Type_STOP_MEASUREMENT), std::runtime_error);
    EXPECT_THROW(write_control(writer, instruction_stream::Control_Type_STOP_MEASUREMENT), std::runtime_error);
    write_control(writer, instruction_stream::Control_Type_START_MEASUREMENT);
    EXPECT_THROW(write_control(writer, instruction_stream::Control_Type_START_MEASUREMENT), std::runtime_error);
    EXPECT_THROW(writer->finalize(), std::runtime_error);
    delete writer;

    ASSERT_EQ(0, unlink(this->test_file_name("is").string().c_str()));
}
