// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "testing/run_tool.h"

#include <string>

#include <gtest/gtest.h>

struct EventStreamViewerTest : perf_streams::testing::CommandTest
{
    EventStreamViewerTest() : CommandTest(BRANCH_STREAM_VIEWER_BIN, "streams") {}
};

TEST_F(EventStreamViewerTest, View)
{
    auto [output, status] = run_command(fixture("test.bs"));
    const auto* expected = R"(Context: type: EL value: 0
Branch: taken   type: UNCONDITIONAL_INDIRECT program counter: 0x00000000deadbeef target: 0xf00ba7
Branch: taken   type: RETURN                 program counter: 0x00007fc9bcb6108c target: 0x7fc9bc980094
Context: type: EL value: 1
SysReg: op0: 1 op1: 3 crn: 7 crm: 3, op2: 4, value: 0xdeadc0def000baa7
Branch: untaken type: CONDITIONAL_DIRECT     program counter: 0x000000fffff00ba8 target: 0x0

total branches = 3
taken branches = 2
conditional direct branches = 1
unconditional direct branches = 0
unconditional indirect branches = 1
call direct branches = 0
call indirect branches = 0
return branches = 1
total sysregs = 1
total context events = 2
)";
    EXPECT_EQ(status, 0);
    EXPECT_EQ(output, expected);
}
