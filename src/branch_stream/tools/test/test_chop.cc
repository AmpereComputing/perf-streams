// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "testing/run_tool.h"

#include <string>

#include "gmock/gmock.h"
#include <gtest/gtest.h>

struct BranchStreamChopTest : perf_streams::testing::CommandTest
{
    BranchStreamChopTest() : CommandTest(BRANCH_STREAM_CHOP_BIN, "streams") {}
};

TEST_F(BranchStreamChopTest, Chop)
{
    auto [output, status] =
        run_command("--force", "--wait", 1, "--capture", 2, fixture("test.bs"), stream_output("test_chopped.bs"));

    ASSERT_EQ(status, 0);
    EXPECT_THAT(output, ::testing::HasSubstr("to 2 branches starting after seeing 1 branches"));
}
