/*
 * Copyright (c) 2026, Ampere Computing LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "event_stream/event_stream.pb.h"
#include "event_stream/processor/bounds.h"

#include <boost/container_hash/hash.hpp>
#include <boost/functional/hash.hpp>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <unordered_map>
#include <utility>
#include <vector>

namespace perf_streams::event_stream::processor {

using FactorKey = std::vector<std::pair<int, uint64_t>>;

inline size_t hash_value(const FactorKey& key)
{
    size_t seed = 0;

    for (const auto& factor : key) {
        boost::hash_combine(seed, factor.first);
        boost::hash_combine(seed, factor.second);
    }

    return seed;
}

using FactoredCountTable = std::unordered_map<FactorKey, int64_t, boost::hash<FactorKey>>;

struct FactorValueMatcher
{
    std::vector<FactorKey::value_type> values;

    bool operator==(const FactorValueMatcher& other) const = default;
};

class FactoredCounts
{
public:
    void add_factor(int definition_id,
                    FactorBounds* bounds,
                    std::optional<FactorValueMatcher> value_matcher = std::nullopt);

    bool increment(const event_stream_proto::Event& event);

    auto factors() const { return std::views::keys(factor_position); }
    const FactoredCountTable& get_factored_counts() const { return counts; }

    bool operator==(const FactoredCounts& other) const;
    bool operator!=(const FactoredCounts& other) const { return !operator==(other); }

private:
    std::unordered_map<int, FactorBounds> factor_bounds;
    std::unordered_map<int, int> factor_position;
    std::unordered_map<int, FactorValueMatcher> factor_value_matchers;
    FactoredCountTable counts;

    bool matches_value_filters(const event_stream_proto::Event& event) const;
    FactorKey to_factor_key(const event_stream_proto::Event& event);

    static FactorKey::value_type to_factor_value(const event_stream_proto::Value& value);
    static FactorKey::value_type::second_type adjust_value(const FactorKey::value_type& value,
                                                           const FactorBounds& bounds);
};

} // namespace perf_streams::event_stream::processor
