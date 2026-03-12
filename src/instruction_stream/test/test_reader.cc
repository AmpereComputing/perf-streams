// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "instruction_stream/instruction_stream.h"

#include <filesystem>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

using std::filesystem::path;
using namespace perf_streams::instruction_stream;

class InstructionStreamReaderTest : public ::testing::Test
{
protected:
    std::string testfixture(std::string filename) { return (path(TEST_DIRECTORY) / "streams" / filename).string(); }
};

TEST_F(InstructionStreamReaderTest, testMissingFile)
{
    ASSERT_THROW(InstructionStreamReader is("doesnotexist.is"), std::system_error);
}

TEST_F(InstructionStreamReaderTest, testMissingXZFile)
{
    ASSERT_THROW(InstructionStreamReader is("doesnotexist.is.xz"), std::system_error);
}

TEST_F(InstructionStreamReaderTest, testRead)
{
    InstructionStreamReader is(testfixture("test.is"));
    Instruction instruction;
    Event event;
    ASSERT_TRUE(is.read(event));
    ASSERT_TRUE(event.has_control());
    ASSERT_TRUE(is.read(instruction));
    ASSERT_TRUE(is.read(instruction));
    ASSERT_TRUE(is.read(event));
    ASSERT_TRUE(event.has_control());
    ASSERT_FALSE(is.read(instruction));
    ASSERT_EQ(is.version(), 6);

    // are the feature bits all false, since we are reading version 1?
    ASSERT_FALSE(is.features().divide_sqrt_registers());
    ASSERT_FALSE(is.features().cache_maintenance_registers());
    ASSERT_FALSE(is.features().memory_translation());
    ASSERT_FALSE(is.features().memory_values());
}

TEST_F(InstructionStreamReaderTest, testReadXZ)
{
    InstructionStreamReader is(testfixture("test.is.xz"));
    Instruction instruction;
    Event event;
    ASSERT_TRUE(is.read(event));
    ASSERT_TRUE(event.has_control());
    ASSERT_TRUE(is.read(instruction));
    ASSERT_TRUE(is.read(instruction));
    ASSERT_TRUE(is.read(event));
    ASSERT_TRUE(event.has_control());
    ASSERT_FALSE(is.read(instruction));
    ASSERT_EQ(is.version(), 6);

    // are the feature bits all false, since we are reading version 1?
    ASSERT_FALSE(is.features().divide_sqrt_registers());
    ASSERT_FALSE(is.features().cache_maintenance_registers());
    ASSERT_FALSE(is.features().memory_translation());
    ASSERT_FALSE(is.features().memory_values());
}
