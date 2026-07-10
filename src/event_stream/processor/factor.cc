// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "factor.h"

#include "event_stream/event_stream.pb.h"
#include "event_stream/processor/bounds.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace perf_streams::event_stream::processor {

namespace {

constexpr size_t max_reserved_count_capacity = 1024;

void normalize_value_matcher(FactorValueMatcher& value_matcher)
{
    std::ranges::sort(value_matcher.values);
    const auto duplicate_values = std::ranges::unique(value_matcher.values);
    value_matcher.values.erase(duplicate_values.begin(), duplicate_values.end());
}

std::optional<size_t> factor_cardinality_for_bounds(const FactorBounds& bounds)
{
    if (!bounds.minimum || !bounds.maximum || *bounds.minimum > *bounds.maximum)
        return std::nullopt;

    const auto minimum = *bounds.minimum;
    const auto maximum = *bounds.maximum;
    if (minimum < 0)
        return std::nullopt;

    const auto span = static_cast<uint64_t>(maximum - minimum);
    if (span > max_reserved_count_capacity)
        return std::nullopt;
    if (bounds.sequence == FactorBounds::EXPONENTIAL && minimum <= 0)
        return std::nullopt;

    std::vector<int64_t> values;
    values.reserve(static_cast<size_t>(span) + 1);

    for (uint64_t offset = 0; offset <= span; ++offset) {
        const auto value = minimum + static_cast<int64_t>(offset);
        const auto adjusted_value = bounds.adjust<int64_t>(value);
        if (!std::ranges::contains(values, adjusted_value))
            values.emplace_back(adjusted_value);
    }

    return values.size();
}

} // namespace

void FactoredCounts::add_factor(int definition_id,
                                FactorBounds* bounds,
                                std::optional<FactorValueMatcher> value_matcher)
{
    if (bounds)
        factor_bounds.emplace(definition_id, *bounds);
    if (value_matcher) {
        normalize_value_matcher(*value_matcher);
        factor_value_matchers.emplace(definition_id, std::move(*value_matcher));
    }
    factor_position.emplace(definition_id, factor_position.size());
}

bool FactoredCounts::increment(const event_stream_proto::Event& event)
{
    if (!matches_value_filters(event))
        return false;

    if (!reserved_count_capacity) {
        reserve_count_capacity();
        reserved_count_capacity = true;
    }

    ++counts[to_factor_key(event)];
    return true;
}

bool FactoredCounts::operator==(const FactoredCounts& other) const
{
    return factor_position == other.factor_position && factor_bounds == other.factor_bounds
           && factor_value_matchers == other.factor_value_matchers;
}

bool FactoredCounts::matches_value_filters(const event_stream_proto::Event& event) const
{
    if (factor_value_matchers.empty())
        return true;

    size_t matched_filters = 0;

    for (const auto& [definition_id, matcher] : factor_value_matchers) {
        bool matched_filter = false;

        for (const auto& value : event.values()) {
            const auto value_definition_id = static_cast<int>(value.definition_id());
            if (value_definition_id != definition_id)
                continue;

            if (!std::ranges::binary_search(matcher.values, to_factor_value(value)))
                return false;

            matched_filter = true;
        }

        if (matched_filter)
            ++matched_filters;
    }

    return matched_filters == factor_value_matchers.size();
}

FactorKey FactoredCounts::to_factor_key(const event_stream_proto::Event& event)
{
    FactorKey key(factor_position.size());

    for (const auto& value : event.values()) {
        if (auto pos = factor_position.find(value.definition_id()); pos != factor_position.end()) {
            auto key_value = to_factor_value(value);
            if (!factor_bounds.empty()) {
                if (auto b = factor_bounds.find(value.definition_id()); b != factor_bounds.end())
                    key_value.second = adjust_value(key_value, b->second);
            }
            key[pos->second] = std::move(key_value);
        }
    }

    return key;
}

FactorKey::value_type FactoredCounts::to_factor_value(const event_stream_proto::Value& value)
{
    if (value.values_case() == event_stream_proto::Value::kIntValue)
        return {-value.definition_id(), value.int_value()};
    else if (value.values_case() == event_stream_proto::Value::kUintValue)
        return {value.definition_id(), value.uint_value()};
    else
        throw std::runtime_error{"factor value is not integral"};
}

FactorKey::value_type::second_type FactoredCounts::adjust_value(const FactorKey::value_type& value,
                                                                const FactorBounds& bounds)
{
    if (value.first < 0)
        return bounds.adjust<int64_t>(static_cast<int64_t>(value.second));
    return bounds.adjust<uint64_t>(value.second);
}

std::optional<size_t> FactoredCounts::factor_cardinality(int definition_id) const
{
    if (const auto value_matcher = factor_value_matchers.find(definition_id);
        value_matcher != factor_value_matchers.end())
        return value_matcher->second.values.size();

    if (const auto bounds = factor_bounds.find(definition_id); bounds != factor_bounds.end())
        return factor_cardinality_for_bounds(bounds->second);

    return std::nullopt;
}

void FactoredCounts::reserve_count_capacity()
{
    size_t count_capacity = 1;

    for (const auto& [definition_id, _] : factor_position) {
        const auto cardinality = factor_cardinality(definition_id);
        if (!cardinality || *cardinality == 0)
            return;
        if (*cardinality > max_reserved_count_capacity || count_capacity > max_reserved_count_capacity / *cardinality)
            return;

        count_capacity *= *cardinality;
    }

    counts.reserve(count_capacity);
}

} // namespace perf_streams::event_stream::processor
