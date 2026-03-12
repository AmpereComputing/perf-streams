/*
 * Copyright (c) 2026, Ampere Computing LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "testing/build_stream.h"

#include <string>

namespace perf_streams::instruction_stream::testing {

inline std::string build_is(const std::string& input)
{
    return ::perf_streams::testing::build_stream(GENERATE_IS_BIN, input, ".is");
}

} // namespace perf_streams::instruction_stream::testing
