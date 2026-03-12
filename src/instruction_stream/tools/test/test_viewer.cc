// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "testing/run_tool.h"

#include <string>

#include <gtest/gtest.h>

struct InstructionStreamViewerTest : perf_streams::testing::CommandTest
{
    InstructionStreamViewerTest() : CommandTest(INSTRUCTION_STREAM_VIEWER_BIN, "streams") {}
};

TEST_F(InstructionStreamViewerTest, View)
{
    auto [output, status] = run_command(fixture("test.is"));
    const auto* expected = R"(Instruction stream is version 6 with the following feature support:
  Memory translation collateral      : false
  Divide/SQRT source registers       : false
  Cache maintenance source registers : false
  SW prefetch source registers       : false
  All destination registers          : false
  Memory values                      : false
  SVE predicated memops              : false
  SVE predicate registers            : false
  Memop size                         : false
  NZCV flags on instructions         : false

Control: type: START_MEASUREMENT
Instruction: inum: 1 pc: 0xffffffff op: 0xffffffff dis: `[undefined opcode: ffffffff]` 
Instruction: inum: 2 pc: 0xeeeeeeee op: 0xeeeeeeee dis: `[undefined opcode: eeeeeeee]` virtual address[0]: 0xaaaaaaaaaaaaaaaa physical address[0]: 0x0 bytes=0 
Control: type: STOP_MEASUREMENT
instruction count = 2
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
