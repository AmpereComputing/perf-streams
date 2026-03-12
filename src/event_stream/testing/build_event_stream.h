/*
 * Copyright (c) 2026, Ampere Computing LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "testing/build_stream.h"

#include <string>

namespace perf_streams::event_stream::testing {

inline std::string build_es(const std::string& input)
{
    return ::perf_streams::testing::build_stream(GENERATE_ES_BIN, input, ".es");
}

inline std::string build_es(const std::string& input, const std::string& output)
{
    return ::perf_streams::testing::build_stream(GENERATE_ES_BIN, input, output);
}

} // namespace perf_streams::event_stream::testing
