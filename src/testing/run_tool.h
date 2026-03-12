/*
 * Copyright (c) 2026, Ampere Computing LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

#include <gtest/gtest.h>

namespace perf_streams::testing {

template<typename... Args>
std::pair<std::string, int> run_tool(const std::string& tool, Args&&... args)
{
    auto full_cmd = fmt::format("{} {} 2>&1", tool, fmt::join(std::forward_as_tuple(std::forward<Args>(args)...), " "));
    std::cout << full_cmd << std::endl;
    auto* cmd_out = popen(full_cmd.c_str(), "r");
    std::stringstream output;
    char buf[512];
    for (;;) {
        auto count = fread(buf, 1, sizeof(buf), cmd_out);
        if (count > 0)
            output.write(buf, count);

        if (count < sizeof(buf))
            break;
    }
    EXPECT_EQ(ferror(cmd_out), 0);
    EXPECT_NE(feof(cmd_out), 0);
    auto status = pclose(cmd_out);
    return {output.str(), status};
}

struct CommandTest : ::testing::Test
{
    std::string tool;
    std::string fixture_path;

    explicit CommandTest(const std::string& tool, const std::string& fixture_path)
        : tool(tool), fixture_path(fixture_path)
    {
    }

    std::string fixture(const std::string& name) const
    {
        return (std::filesystem::path(TEST_DIRECTORY) / fixture_path / name).string();
    }

    std::string stream_output(const std::string& name) const
    {
        return (std::filesystem::path(TEST_OUTPUT_DIRECTORY) / name).string();
    }

    template<typename... Args>
    auto run_command(Args&&... args) const
    {
        return run_tool(tool, std::forward<Args>(args)...);
    }
};

} // namespace perf_streams::testing
