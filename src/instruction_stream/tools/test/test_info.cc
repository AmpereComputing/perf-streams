// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "testing/run_tool.h"

#include <string>

#include <gtest/gtest.h>

struct InstructionStreamInfoTest : perf_streams::testing::CommandTest
{
    InstructionStreamInfoTest() : CommandTest(INSTRUCTION_STREAM_INFO_BIN, "streams") {}
};

TEST_F(InstructionStreamInfoTest, Info)
{
    auto [output, status] = run_command(fixture("test.is"));
    const auto* expected = R"(stream version = 6
total instruction count = 2
  (0 warmup, 2 measurement, and 0 cooldown)
memop count = 1
memory count = 0
sysreg count = 0
context count = 0
control count = 2
total event count = 4
)";
    EXPECT_EQ(status, 0);
    EXPECT_EQ(output, expected);
}
