// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include <fcntl.h>
#include <memory>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

#include <gtest/gtest.h>

// FIXME:
#include "branch_stream/branch_stream.h"
#include "protobuf_utils/protobuf_utils.h"
#include "testing/writer.h"

#include <google/protobuf/io/zero_copy_stream_impl.h>

using namespace perf_streams;

class BranchStreamWriterTest : public perf_streams::testing::StreamWriterTest
{};

TEST_F(BranchStreamWriterTest, Uncompressed)
{
    output_stream_name = this->test_file_name("bs");
    {
        auto writer = std::make_unique<branch_stream::BranchStreamWriter>(output_stream_name.string().c_str());

        instruction_stream::Context context;
        context.set_type(instruction_stream::Context_Type_EL);
        context.set_value(0);
        ASSERT_TRUE(writer->write(context));

        branch_stream::Branch branch;
        branch.set_type(branch_stream::Branch_Type_UNCONDITIONAL_INDIRECT);
        branch.set_taken(true);
        branch.set_program_counter(0xdeadbeef);
        branch.set_target(0xf00ba7);
        ASSERT_TRUE(writer->write(branch));

        branch.set_type(branch_stream::Branch_Type_RETURN);
        branch.set_taken(true);
        branch.set_program_counter(0x7fc9bcb6108c);
        branch.set_target(0x7fc9bc980094);
        ASSERT_TRUE(writer->write(branch));

        context.set_type(instruction_stream::Context_Type_EL);
        context.set_value(1);
        ASSERT_TRUE(writer->write(context));

        instruction_stream::SysReg sysreg;
        sysreg.set_type(instruction_stream::SysReg_Type_WRITE);
        sysreg.set_value(0xdeadc0def000baa7);
        ASSERT_TRUE(writer->write(sysreg));

        branch.set_type(branch_stream::Branch_Type_CONDITIONAL_DIRECT);
        branch.set_taken(false);
        branch.set_program_counter(0xfffff00ba8);
        branch.clear_target();
        ASSERT_TRUE(writer->write(branch));
    }

    int fd = open(this->test_file_name("bs").string().c_str(), O_RDONLY);
    ASSERT_GT(fd, 2);

    {
        auto input_stream = std::make_unique<google::protobuf::io::FileInputStream>(fd);

        uint32_t magic, bs_version;
        perf_streams::protobuf_utils::read_header_from(input_stream, &magic, &bs_version);
        ASSERT_EQ(magic, 0x53424250); // 0x50(P) 0x42(B) 0x42(B) 0x53(S));
        ASSERT_EQ(bs_version, 1);

        branch_stream::Event event;
        ASSERT_TRUE(perf_streams::protobuf_utils::read_delimited_from(input_stream, &event, nullptr));
        ASSERT_TRUE(event.has_context());
        ASSERT_EQ(instruction_stream::Context_Type_EL, event.context().type());
        ASSERT_EQ(0, event.context().value());

        ASSERT_TRUE(perf_streams::protobuf_utils::read_delimited_from(input_stream, &event, nullptr));
        ASSERT_TRUE(event.has_branch());
        ASSERT_EQ(branch_stream::Branch_Type_UNCONDITIONAL_INDIRECT, event.branch().type());
        ASSERT_EQ(true, event.branch().taken());
        ASSERT_EQ(0xdeadbeef, event.branch().program_counter());
        ASSERT_EQ(0xf00ba7, event.branch().target());

        ASSERT_TRUE(perf_streams::protobuf_utils::read_delimited_from(input_stream, &event, nullptr));
        ASSERT_TRUE(event.has_branch());
        ASSERT_EQ(branch_stream::Branch_Type_RETURN, event.branch().type());
        ASSERT_EQ(true, event.branch().taken());
        ASSERT_EQ(0x7fc9bcb6108c, event.branch().program_counter());
        ASSERT_EQ(0x7fc9bc980094, event.branch().target());

        ASSERT_TRUE(perf_streams::protobuf_utils::read_delimited_from(input_stream, &event, nullptr));
        ASSERT_TRUE(event.has_context());
        ASSERT_EQ(instruction_stream::Context_Type_EL, event.context().type());
        ASSERT_EQ(1, event.context().value());

        ASSERT_TRUE(perf_streams::protobuf_utils::read_delimited_from(input_stream, &event, nullptr));
        ASSERT_TRUE(event.has_sysreg());
        ASSERT_EQ(instruction_stream::SysReg_Type_WRITE, event.sysreg().type());
        ASSERT_EQ(0xdeadc0def000baa7, event.sysreg().value());

        ASSERT_TRUE(perf_streams::protobuf_utils::read_delimited_from(input_stream, &event, nullptr));
        ASSERT_TRUE(event.has_branch());
        ASSERT_EQ(branch_stream::Branch_Type_CONDITIONAL_DIRECT, event.branch().type());
        ASSERT_EQ(false, event.branch().taken());
        ASSERT_EQ(0xfffff00ba8, event.branch().program_counter());
        ASSERT_EQ(0, event.branch().target());

        ASSERT_TRUE(input_stream->Close());
    }
}

TEST_F(BranchStreamWriterTest, Compressed)
{
    output_stream_name = this->test_file_name("bs.xz");
    {
        auto writer = std::make_unique<branch_stream::BranchStreamWriter>(output_stream_name.string().c_str());

        instruction_stream::Context context;
        context.set_type(instruction_stream::Context_Type_EL);
        context.set_value(0);
        ASSERT_TRUE(writer->write(context));

        branch_stream::Branch branch;
        branch.set_type(branch_stream::Branch_Type_UNCONDITIONAL_INDIRECT);
        branch.set_taken(true);
        branch.set_program_counter(0xdeadbeef);
        branch.set_target(0xf00ba7);
        ASSERT_TRUE(writer->write(branch));

        branch.set_type(branch_stream::Branch_Type_RETURN);
        branch.set_taken(true);
        branch.set_program_counter(0x7fc9bcb6108c);
        branch.set_target(0x7fc9bc980094);
        ASSERT_TRUE(writer->write(branch));

        context.set_type(instruction_stream::Context_Type_EL);
        context.set_value(1);
        ASSERT_TRUE(writer->write(context));

        instruction_stream::SysReg sysreg;
        sysreg.set_type(instruction_stream::SysReg_Type_WRITE);
        sysreg.set_value(0xdeadc0def000baa7);
        ASSERT_TRUE(writer->write(sysreg));

        branch.set_type(branch_stream::Branch_Type_CONDITIONAL_DIRECT);
        branch.set_taken(false);
        branch.set_program_counter(0xfffff00ba8);
        branch.clear_target();
        ASSERT_TRUE(writer->write(branch));
    }

    {
        auto reader = std::make_unique<branch_stream::BranchStreamReader>(test_file_name("bs.xz"));

        ASSERT_EQ(reader->version(), 1);

        branch_stream::Event event;
        ASSERT_TRUE(reader->read(event));
        ASSERT_TRUE(event.has_context());
        ASSERT_EQ(instruction_stream::Context_Type_EL, event.context().type());
        ASSERT_EQ(0, event.context().value());

        branch_stream::Branch branch;
        ASSERT_TRUE(reader->read(branch));
        ASSERT_EQ(branch_stream::Branch_Type_UNCONDITIONAL_INDIRECT, branch.type());
        ASSERT_EQ(true, branch.taken());
        ASSERT_EQ(0xdeadbeef, branch.program_counter());
        ASSERT_EQ(0xf00ba7, branch.target());

        ASSERT_TRUE(reader->read(branch));
        ASSERT_EQ(branch_stream::Branch_Type_RETURN, branch.type());
        ASSERT_EQ(true, branch.taken());
        ASSERT_EQ(0x7fc9bcb6108c, branch.program_counter());
        ASSERT_EQ(0x7fc9bc980094, branch.target());

        ASSERT_TRUE(reader->read(event));
        ASSERT_TRUE(event.has_context());
        ASSERT_EQ(instruction_stream::Context_Type_EL, event.context().type());
        ASSERT_EQ(1, event.context().value());

        ASSERT_TRUE(reader->read(event));
        ASSERT_TRUE(event.has_sysreg());
        ASSERT_EQ(instruction_stream::SysReg_Type_WRITE, event.sysreg().type());
        ASSERT_EQ(0xdeadc0def000baa7, event.sysreg().value());

        ASSERT_TRUE(reader->read(event));
        ASSERT_TRUE(event.has_branch());
        ASSERT_EQ(branch_stream::Branch_Type_CONDITIONAL_DIRECT, event.branch().type());
        ASSERT_EQ(false, event.branch().taken());
        ASSERT_EQ(0xfffff00ba8, event.branch().program_counter());
        ASSERT_EQ(0, event.branch().target());
    }
}
