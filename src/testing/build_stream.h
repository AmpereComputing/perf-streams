/*
 * Copyright (c) 2026, Ampere Computing LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstddef>
#include <cstdlib>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <stdexcept>
#include <string>

namespace perf_streams::testing {

inline std::string build_stream(const std::string& generator,
                                const std::string& input,
                                const std::string& suffix_or_output)
{
    auto output = suffix_or_output;
    if (suffix_or_output.starts_with('.')) {
        std::string prefix;
        if (auto dot = input.rfind('.'); dot != std::string::npos)
            prefix = input.substr(0, dot);
        else
            prefix = input;

        output = prefix + suffix_or_output;
    }
    if (auto r = std::system(fmt::format("{} {} {}", generator, input, output).c_str()); r != 0)
        throw std::runtime_error(fmt::format("Error running {}. retval = {}", generator, r));

    return output;
}

} // namespace perf_streams::testing
