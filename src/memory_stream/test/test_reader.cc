// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "memory_stream/memory_stream.h"

#include <filesystem>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

using std::filesystem::path;
using namespace perf_streams::memory_stream;

class MemoryStreamReaderTest : public ::testing::Test
{
protected:
    std::string testfixture(std::string filename) { return (path(TEST_DIRECTORY) / "streams" / filename).string(); }
};

TEST_F(MemoryStreamReaderTest, MissingFile)
{
    ASSERT_THROW(const MemoryStreamReader ms("doesnotexist.ms"), std::system_error);
}

TEST_F(MemoryStreamReaderTest, MissingXZFile)
{
    ASSERT_THROW(const MemoryStreamReader ms("doesnotexist.ms.xz"), std::system_error);
}

TEST_F(MemoryStreamReaderTest, Read)
{
    MemoryStreamReader ms(testfixture("test.ms"));
    Access access;
    ASSERT_TRUE(ms.read(access));
    ASSERT_TRUE(ms.read(access));
    ASSERT_FALSE(ms.read(access));
    ASSERT_EQ(ms.version(), 1);
}

TEST_F(MemoryStreamReaderTest, ReadXZ)
{
    MemoryStreamReader ms(testfixture("test.ms.xz"));
    Access access;
    ASSERT_TRUE(ms.read(access));
    ASSERT_TRUE(ms.read(access));
    ASSERT_FALSE(ms.read(access));
    ASSERT_EQ(ms.version(), 1);
}

TEST_F(MemoryStreamReaderTest, ReadVersion2)
{
    MemoryStreamReader ms(testfixture("test_v2.ms.xz"));
    Access access;
    Event event;
    ASSERT_TRUE(ms.read(event));
    ASSERT_TRUE(event.has_control());
    ASSERT_TRUE(Control_Type_START_MEMORY == event.control().type());
    ASSERT_TRUE(ms.read(event));
    ASSERT_TRUE(event.has_access());
    ASSERT_TRUE(ms.read(access));
    ASSERT_TRUE(ms.read(access));
    ASSERT_TRUE(ms.read(access));
    ASSERT_TRUE(ms.read(access));
    ASSERT_TRUE(ms.read(access));
    ASSERT_TRUE(ms.read(access));
    ASSERT_TRUE(ms.read(access));
    ASSERT_TRUE(ms.read(access));
    ASSERT_EQ(ms.version(), 2);
    ASSERT_TRUE(ms.filter_was_unified());
    ASSERT_EQ(ms.filter_dcache_size(), 2097152);
    ASSERT_EQ(ms.filter_icache_size(), 2097152);
}

TEST_F(MemoryStreamReaderTest, ReadVersion3)
{
    MemoryStreamReader ms(testfixture("test_v3.ms.xz"));
    Access access;
    Event event;
    ASSERT_TRUE(ms.read(event));
    ASSERT_TRUE(event.has_control());
    ASSERT_TRUE(Control_Type_START_MEMORY == event.control().type());
    ASSERT_TRUE(ms.read(event));
    ASSERT_TRUE(event.has_access());
    ASSERT_TRUE(ms.read(access));
    ASSERT_TRUE(ms.read(access));
    ASSERT_TRUE(ms.read(access));
    ASSERT_TRUE(ms.read(access));
    ASSERT_TRUE(ms.read(access));
    ASSERT_TRUE(ms.read(access));
    ASSERT_TRUE(ms.read(access));
    ASSERT_TRUE(ms.read(access));
    ASSERT_EQ(ms.version(), 3);
    ASSERT_FALSE(ms.filter_was_unified());
    ASSERT_EQ(ms.filter_dcache_size(), 65536);
    ASSERT_EQ(ms.filter_icache_size(), 32768);
}
