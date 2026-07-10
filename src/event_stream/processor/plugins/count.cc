// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "event_stream/processor/args.h"
#include "event_stream/processor/counter.h"
#include "event_stream/processor/factor.h"
#include "event_stream/processor/metric_table.h"
#include "event_stream/processor/plugin.h"
#include "event_stream/processor/processor_ifc.h"

#include <cstdint>
#include <cstdlib>
#include <fmt/ostream.h>
#include <fmt/ranges.h>
#include <ranges>
#include <set>
#include <sstream>
#include <string>

namespace perf_streams::event_stream::processor {

EVP_PLUGIN(Count, "count", "Count events and/or event data values")
{
public:
    Count(ProcessorIfc & proc_ifc, Args & args);

    static void help(int argc, const char** argv);
    static bool singular()
    {
        return true;
    }

    void start_simulation() override;
    void collect(MetricSeries & metrics, uint64_t trigger_time) override;
    std::set<Phase> phases() const override
    {
        if (list_events)
            return {Phase::DEFINITIONS};
        return {Phase::COUNTERS};
    }

private:
    bool expand_enumerations = true;
    bool list_events = false;

    CounterSet counters;

    std::string counter_name(const Counter& counter) const;
    std::string factor_name(const std::string& name, const FactorKey& factor_key) const;
};

Count::Count(ProcessorIfc& proc_ifc, Args& args) : Plugin{proc_ifc}
{
    while (args) {
        std::string arg;

        if (args.pop("-e", arg))
            count(arg, &counters);
        else if (args.pop("-x", arg))
            count(arg, &counters, CountAction::REMOVE);
        else if (args.pop("-a"))
            count("*", &counters);
        else if (args.pop("--accumulate", arg))
            accumulate(arg, &counters);
        else if (args.pop("--no-enum"))
            expand_enumerations = false;
        else if (args.pop("--list"))
            list_events = true;
        else
            break;
    }

    args.done();
}

void Count::help(int argc, const char** argv)
{
    print_help(
        argv[0], "count", "[-a] [-e <event>] [-x <event>] [--accumulate <event data>] [--enum] [--list]", R"(Arguments:

    -a                          Count all events.
    -e <event>                  Count given event.
                                Factors can be selected with: -e */factor_name.
                                Factor values filter counts with: -e */factor_name:value.
                                Values may be numeric or enum names.
                                Unvalued factors can be mixed with value filters.
                                Value-filtered counters omit the base event metric.
                                Additionally, factor values can be adjusted with: -e */factor_name[min:max:granularity].
    -x <event>                  Exclude counting a given event (or factor).
    --accumulate <event data>   Accumulate data value for event.
    --no-enum                   Do not expand enumerations factors into string values
    --list                      List all events/data getting counted (and exit)
    --help, -h                  This help message.
)");
}

std::string Count::counter_name(const Counter& counter) const
{
    if (counter.data_definition_id) {
        return fmt::format("{}/{}",
                           get_definition(counter.event_definition_id).name(),
                           get_definition(counter.data_definition_id).name());
    }

    return get_definition(counter.event_definition_id).name();
}

std::string Count::factor_name(const std::string& name, const FactorKey& factor_key) const
{
    int added_factors = 0;
    std::stringstream ss;
    ss << name;

    // FIXME: Ugly factoring internals exposed!
    for (auto [data_def_id, data_value] : factor_key) {
        if (data_def_id == 0)
            continue;

        const auto& data_def = get_definition(data_def_id < 0 ? -data_def_id : data_def_id);

        ss << '/' << data_def.name() << ':';
        const auto* name = expand_enumerations ? enumeration_value_for_definition(data_def, data_value) : nullptr;
        if (name)
            ss << name;
        else if (data_def_id < 0)
            ss << static_cast<int64_t>(data_value);
        else
            ss << data_value;

        ++added_factors;
    }

    if (added_factors == 0)
        ss << '/';

    return ss.str();
}

void Count::start_simulation()
{
    if (!list_events)
        return;

    for (auto counter_id : counters) {
        const auto& counter = get_counter(counter_id);
        auto name = counter_name(counter);
        if (counter.factored()) {
            fmt::print("factor {}/{}\n",
                       name,
                       fmt::join(std::views::transform(counter.factors(),
                                                       [&](const auto id) { return get_definition(id).name(); }),
                                 "/"));
        } else {
            fmt::print("event  {}\n", name);
        }
    }

    exit(0);
}

void Count::collect(MetricSeries& metrics, uint64_t trigger_time)
{
    for (auto counter_id : counters) {
        const auto& counter = get_counter(counter_id);
        auto name = counter_name(counter);
        if (counter.collected())
            metrics[name] = counter.get_count();

        if (const auto* factored_counts = counter.get_factored_counts(); factored_counts) {
            for (const auto& [factor_key, count] : *factored_counts) {
                if (!factor_key.empty())
                    metrics[factor_name(name, factor_key)] = count;
            }
        }
    }
}

} // namespace perf_streams::event_stream::processor
