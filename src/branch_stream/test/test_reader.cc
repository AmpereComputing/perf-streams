// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "branch_stream/branch_stream.h"

#include <filesystem>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

using std::filesystem::path;
using namespace perf_streams::branch_stream;

class BranchStreamReaderTest : public ::testing::Test
{
protected:
    Branch branch;
    Event event;

    std::string testfixture(std::string filename) { return (path(TEST_DIRECTORY) / "streams" / filename).string(); }
};

TEST_F(BranchStreamReaderTest, MissingFile)
{
    ASSERT_THROW(const BranchStreamReader bs("doesnotexist.bs"), std::system_error);
}

TEST_F(BranchStreamReaderTest, MissingXZFile)
{
    ASSERT_THROW(const BranchStreamReader bs("doesnotexist.bs.xz"), std::system_error);
}

TEST_F(BranchStreamReaderTest, Read)
{
    BranchStreamReader bs(testfixture("test.bs"));
    ASSERT_TRUE(bs.read(branch));
    ASSERT_TRUE(bs.read(branch));
    ASSERT_TRUE(bs.read(branch));
    ASSERT_FALSE(bs.read(branch));
    ASSERT_EQ(bs.version(), 1);
}

TEST_F(BranchStreamReaderTest, ReadXZBranchesOnly)
{
    BranchStreamReader bs(testfixture("test.bs.xz"));
    ASSERT_TRUE(bs.read(branch));
    ASSERT_TRUE(bs.read(branch));
    ASSERT_TRUE(bs.read(branch));
    ASSERT_FALSE(bs.read(branch));
    ASSERT_EQ(bs.version(), 1);
}

TEST_F(BranchStreamReaderTest, ReadXZAllEvents)
{
    BranchStreamReader bs(testfixture("test.bs.xz"));
    ASSERT_TRUE(bs.read(event));
    ASSERT_TRUE(bs.read(event));
    ASSERT_TRUE(bs.read(event));
    ASSERT_TRUE(bs.read(event));
    ASSERT_TRUE(bs.read(event));
    ASSERT_TRUE(bs.read(event));
    ASSERT_FALSE(bs.read(event));
    ASSERT_EQ(bs.version(), 1);
}
