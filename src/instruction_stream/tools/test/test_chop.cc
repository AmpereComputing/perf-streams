// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "testing/run_tool.h"

#include <string>

#include "gmock/gmock.h"
#include <gtest/gtest.h>

struct InstructionStreamChopTest : perf_streams::testing::CommandTest
{
    InstructionStreamChopTest() : CommandTest(INSTRUCTION_STREAM_CHOP_BIN, "streams") {}
};

TEST_F(InstructionStreamChopTest, Chop)
{
    // clang-format off
    auto [output, status] = run_command("--force",
                                        "--warmup", 1,
                                        "--measure", 1,
                                        "--cooldown", 0,
                                        fixture("test.is"),
                                        stream_output("test_chopped.is"));
    // clang-format on

    ASSERT_EQ(status, 0);
    EXPECT_THAT(output, ::testing::HasSubstr("to 1 instructions with 1 instructions of warmup"));

    auto [info_output, info_status] =
        perf_streams::testing::run_tool(INSTRUCTION_STREAM_INFO_BIN, stream_output("test_chopped.is"));
    const auto* expected = R"(stream version = 6
total instruction count = 2
  (1 warmup, 1 measurement, and 0 cooldown)
memop count = 1
memory count = 0
sysreg count = 0
context count = 0
control count = 2
total event count = 4
)";
    EXPECT_EQ(info_status, 0);
    EXPECT_EQ(info_output, expected);
}
