// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "testing/run_tool.h"

#include <string>

#include <gtest/gtest.h>

struct MemoryStreamViewerTest : perf_streams::testing::CommandTest
{
    MemoryStreamViewerTest() : CommandTest(MEMORY_STREAM_VIEWER_BIN, "streams") {}
};

TEST_F(MemoryStreamViewerTest, View)
{
    auto [output, status] = run_command(fixture("test.ms"));
    const auto* expected = R"(Memory stream is version 1

OTHR READ      : physical address: 0x8 virtual address: 0x0 size: 3 offset: 0 originator id: 0 program counter: 0x0 
OTHR WRITE     : physical address: 0x4 virtual address: 0x0 size: 98 offset: 3 originator id: 549755781892 program counter: 0x0 

Note: Accesses filtered through unified 1024-byte cache

total access count = 2
data count = 0
code count = 0
read count = 1
write count = 1
readunique count = 0
total bytes count = 101
bytes read count = 3
bytes write count = 98
bytes readunique count = 0
unique lines = 2
)";
    EXPECT_EQ(status, 0);
    EXPECT_EQ(output, expected);
}

