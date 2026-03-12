/*
 * Copyright (c) 2026, Ampere Computing LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>
#include <fmt/base.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

extern "C" {
#include "strnatcmp.h"
}

#include "frame.h"

namespace perf_streams::event_stream::processor {

struct NatComp
{
    bool operator()(const std::string& a, const std::string& b) const { return strnatcmp(a.c_str(), b.c_str()) < 0; }
};

// represents the null value
struct NAType
{};
extern NAType NA;

using MetricValue = std::variant<NAType, int64_t, uint64_t, double, std::string>;

struct metric_frame_traits : public FrameTraits<MetricValue, std::vector<MetricValue>, std::string, NatComp>
{};

using MetricSeries = RowSeries<MetricValue, metric_frame_traits>;
using MetricColumn = ColSeries<MetricValue, metric_frame_traits>;
using MetricTable = Frame<MetricValue, metric_frame_traits>;

struct MetricTableTimeSeries : public MetricTable
{
    std::vector<uint64_t> times;
};

} // namespace perf_streams::event_stream::processor

template<>
struct fmt::formatter<perf_streams::event_stream::processor::NAType> : fmt::formatter<std::string_view>
{
    template<typename FormatContext>
    auto format(perf_streams::event_stream::processor::NAType c, FormatContext& ctx) const
    {
        return fmt::formatter<std::string_view>::format("NA", ctx);
    }
};

template<>
struct fmt::formatter<perf_streams::event_stream::processor::MetricValue> : fmt::ostream_formatter
{};
