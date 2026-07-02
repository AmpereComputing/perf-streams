// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "event_stream/processor/args.h"
#include "event_stream/processor/bounds.h"
#include "event_stream/processor/counter.h"
#include "event_stream/processor/metric_table.h"
#include "event_stream/processor/plugins/interval_histogram.h"
#include "event_stream/processor/processor_ifc.h"

#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

namespace perf_streams::event_stream::processor {

EVP_PLUGIN_FROM(Rate, "rate", "Count events with some time-slice", IntervalHistogramPlugin)
{
public:
    Rate(ProcessorIfc & proc_ifc, Args & args);

    static void help(int argc, const char** argv);
    void collect_within_event(Counter * counter, const Event& event);
    void collect_within_time(uint64_t current_time, uint64_t expiry);
    void collect(MetricSeries & metrics, uint64_t trigger_time) override;
    std::set<Phase> phases() const override
    {
        return {Phase::EVENTS};
    }

private:
    void collect_within();
    void collect_empty_time_intervals(uint64_t count);

    struct Setting
    {
        int64_t previous_value = 0;
        std::unique_ptr<FactorBounds> bounds;
        explicit Setting(std::unique_ptr<FactorBounds>&& bounds) : bounds(std::move(bounds)) {}
    };
    std::list<std::pair<std::string, CounterSet>> counters;
    std::map<int, Setting> settings;
    std::map<int, std::map<int64_t, uint64_t>> histogram;
};

Rate::Rate(ProcessorIfc& proc_ifc, Args& args) : IntervalHistogramPlugin{proc_ifc}
{
    std::string name;
    std::string interval;
    args.pop(interval);

    while (args) {
        std::string arg;
        if (args.pop("--factored")) {
            set_factored_metrics(true);
        } else if (args.pop("-n|--name", name)) {
            counters.emplace_back(name, CounterSet{});
        } else if (args.pop("-e|--event", arg)) {
            if (counters.empty())
                counters.emplace_back("rate", CounterSet{});

            auto [event_name, bounds] = FactorBounds::from_event_spec(arg);
            CounterSet event_counters;
            count(event_name, &event_counters);
            for (auto id : event_counters)
                settings.emplace(id, std::move(bounds));

            counters.back().second.merge(event_counters);
        } else
            break;
    }

    args.done();

    configure_interval(interval, "rate", &Rate::collect_within_event, &Rate::collect_within_time);
}

void Rate::collect_within()
{
    for (auto& [counter_id, setting] : settings) {
        const auto& counter = get_counter(counter_id);
        auto current_value = counter.get_count();
        auto within_period = current_value - setting.previous_value;
        if (setting.bounds)
            within_period = setting.bounds->adjust(within_period);

        setting.previous_value = current_value;
        ++histogram[counter_id][within_period];
    }
}

void Rate::collect_empty_time_intervals(uint64_t count)
{
    if (count == 0)
        return;

    for (auto& [counter_id, setting] : settings) {
        int64_t within_period = 0;
        if (setting.bounds)
            within_period = setting.bounds->adjust(within_period);

        histogram[counter_id][within_period] += count;
    }
}

void Rate::collect_within_event(Counter* counter, const Event& event)
{
    collect_within();
    mark_interval_update(event.time());
}

void Rate::collect_within_time(uint64_t current_time, uint64_t expiry)
{
    collect_within();
    collect_empty_time_intervals(empty_time_boundaries_after(current_time, expiry));
    mark_interval_update(latest_time_boundary(current_time, expiry));
}

void Rate::collect(MetricSeries& metrics, uint64_t trigger_time)
{
    finish_final_interval(trigger_time, [this] { collect_within(); });

    for (auto& [name, name_counters] : counters) {
        for (auto counter_id : name_counters) {
            const auto& counter = get_counter(counter_id);
            const auto& event_name = get_definition(counter.event_definition_id).name();

            for (auto [rate, count] : histogram[counter_id])
                metrics[format_histogram_bucket(event_name, name, name, rate)] = count;
        }
    }
}

void Rate::help(int argc, const char** argv)
{
    print_help(argv[0], "rate", "<interval> [--factored] [-n|--name <name>] [-e|--event <event>]", R"(Arguments:

    <interval>          Time interval (with optional ps/ns/us/ms/s suffix, like -i) or event to consider
                        beginning of rate measurement. Time intervals are grouped by time / interval,
                        and empty time intervals contribute to the 0 bucket.
    --factored          Emit events as factored instead of suffix
    -n, --name          Event suffix or data-value name for output event
    -e, --event         Metric name for results, with optional bucket/bounds settings similar to +count, like:
                        event_name[min:max:granularity].
    --help, -h          This help message.
)");
}

} // namespace perf_streams::event_stream::processor
