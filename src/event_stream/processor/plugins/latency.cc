// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "event_stream/processor/plugins/latency.h"

#include "event_stream/event_stream.pb.h"
#include "event_stream/processor/args.h"
#include "event_stream/processor/bounds.h"
#include "event_stream/processor/counter.h"
#include "event_stream/processor/metric_table.h"
#include "event_stream/processor/plugin.h"
#include "event_stream/processor/processor_ifc.h"
#include "event_stream/processor/utils.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fmt/core.h>
#include <fmt/format.h>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace perf_streams::event_stream::processor {

EVP_PLUGIN(Latency, "latency", "Measure time between pairs of events in transaction")
{
public:
    Latency(ProcessorIfc & proc_ifc, Args & args);

    static void help(int argc, const char** argv);
    void define_value(const Definition& value_def) override;
    void collect(MetricSeries & metrics, uint64_t trigger_time) override;
    std::set<Phase> phases() const override
    {
        return {Phase::EVENTS};
    }

private:
    bool enable{true};

    size_t max_latencies_vector_size{10};

    using TimeCount = plugins::TimeCount;
    using LatencyHist = plugins::BasicLatencyHist<float>;
    using LatencyTracker = plugins::LatencyTracker;
    using LatencyInfo = plugins::LatencyInfo;
    using LatencyHistInfo = std::vector<LatencyHist>;

    std::vector<std::string> events;
    std::vector<std::string> event_aliases;
    std::string name;
    std::unordered_set<std::string> key_data_names{{"txid"}};

    LatencyInfo aggregated_info;
    LatencyHistInfo aggregated_hist_info;

    std::unordered_map<uint64_t, LatencyTracker> trackers;
    using TrackerIter = decltype(trackers)::iterator;

    bool histogram{false};
    std::string histogram_metric_name;
    std::unique_ptr<FactorBounds> histogram_bounds;
    bool factored_histogram{false};

    bool tracking_transactions{true};
    std::unordered_set<int> key_definitions;

    void update_latency(Counter * counter, const event_stream_proto::Event& event, int idx);
    void end_transaction(Counter * counter, const event_stream_proto::Event& event);
    void finalize_latency(const TrackerIter& tracker);
    std::optional<uint64_t> get_key(const event_stream_proto::Event& event) const;
    TrackerIter get_tracker_by_key(const event_stream_proto::Event& event);
};

Latency::Latency(ProcessorIfc& proc_ifc, Args& args) : Plugin{proc_ifc}
{
    std::string prefix{};
    bool ignore_missing{false};

    while (args) {
        std::string event;

        if (args.pop("-n|--name", name)) {
        } else if (args.pop("-p|--prefix", prefix)) {
        } else if (args.pop("-k|--key", event)) {
            if (tracking_transactions) {
                key_data_names.clear();
                tracking_transactions = false;
            }
            key_data_names.emplace(event);
        } else if (args.pop("--ignore-missing")) {
            ignore_missing = true;
        } else if (args.pop("--histogram", event)) {
            histogram = true;
            std::tie(histogram_metric_name, histogram_bounds) = FactorBounds::from_event_spec(event);
        } else if (args.pop("--factored")) {
            factored_histogram = true;
        } else if (args.pop(event)) {
            events.emplace_back(event);
        } else {
            break;
        }
    }

    args.done();

    if (!prefix.empty() && !tracking_transactions) {
        auto original_names = std::move(key_data_names);
        key_data_names.clear();
        for (const auto& key : original_names)
            key_data_names.emplace(fmt::format("{}.{}", prefix, key));
    }

    aggregated_info.resize(events.size());
    aggregated_hist_info.resize(events.size());

    int idx = 0;

    for (auto& event : events) {
        auto pos = event.find("=");

        if (pos != std::string::npos) {
            event_aliases.push_back(event.substr(pos + 1));
            event = event.substr(0, pos);
        } else {
            event_aliases.push_back(event);
        }

        auto event_name = prefix.empty() ? event : fmt::format("{}.{}", prefix, event);

        auto update = [this, idx](Counter* counter, const event_stream_proto::Event& event) {
            this->update_latency(counter, event, idx);
        };

        try {
            on_every(event_name, update);
        } catch (NoSuchDefinition& exc) {
            if (!ignore_missing)
                throw;

            if (idx == 0)
                enable = false;
        }

        ++idx;
    }

    if (enable) {
        if (name.empty())
            name = plugins::build_latency_name(events);

        if (tracking_transactions)
            on_every("end_transaction", &Latency::end_transaction);
    }
}

void Latency::help(int argc, const char** argv)
{
    print_help(argv[0], "latency", "[-n|--name <name>] [-p|--prefix <prefix>] <start event> <stop event>", R"(Arguments:

    <start event>          Event to consider beginning of latency measurement
    <end event>            Event to consider end of latency measurement
    -n, --name             Metric name for results
    -p, --prefix <event>   Prefix applied to start and end event
    -k, --key <data-type>  Data to use for event tracking (default: txid)
    --histogram <name>     Emit histogram and use suffix name for results, with optional bucket/bounds settings similar to +count,
                           like: name[min:max:granularity].
    --factored             Emit histogram as factored instead of suffix
    --help, -h             This help message.
)");
}

void Latency::define_value(const Definition& value_def)
{
    if (key_data_names.contains(value_def.name()))
        key_definitions.emplace(value_def.id());
}

void Latency::update_latency(Counter* counter, const event_stream_proto::Event& event, int idx)
{
    if (auto key = get_key(event); key) {
        auto tracker_iter = trackers.find(*key);
        if (tracker_iter == trackers.end())
            std::tie(tracker_iter, std::ignore) = trackers.insert({*key, LatencyTracker(event.time(), events.size())});

        plugins::record_latency(tracker_iter->second, event.time(), idx);

        if (!tracking_transactions && static_cast<size_t>(idx + 1) == events.size())
            finalize_latency(tracker_iter);
    }
}

void Latency::finalize_latency(const TrackerIter& tracker_iter)
{
    plugins::aggregate_latency(tracker_iter->second,
                               aggregated_info,
                               aggregated_hist_info,
                               histogram_bounds.get(),
                               histogram,
                               max_latencies_vector_size);
    trackers.erase(tracker_iter);
}

void Latency::end_transaction(Counter* counter, const event_stream_proto::Event& event)
{
    if (auto tracker_iter = get_tracker_by_key(event); tracker_iter != trackers.end())
        finalize_latency(tracker_iter);
}

std::optional<uint64_t> Latency::get_key(const event_stream_proto::Event& event) const
{
    for (int i = 0; i < event.values_size(); ++i) {
        const auto& value = event.values(i);
        if (key_definitions.contains(value.definition_id()))
            return value.uint_value();
    }

    return {};
}

Latency::TrackerIter Latency::get_tracker_by_key(const event_stream_proto::Event& event)
{
    if (auto txid = get_key(event); txid)
        return trackers.find(*txid);

    return trackers.end();
}

void Latency::collect(MetricSeries& metrics, uint64_t trigger_time)
{
    if (!enable)
        return;

    plugins::assign_latency_metrics(metrics,
                                    name,
                                    aggregated_info[0],
                                    aggregated_hist_info[0],
                                    histogram_metric_name,
                                    factored_histogram,
                                    "",
                                    histogram);

    if (events.size() > 2) {
        for (size_t i = 0; i < events.size() - 1; ++i)
            plugins::assign_latency_metrics(metrics,
                                            name,
                                            aggregated_info[i],
                                            aggregated_hist_info[i],
                                            histogram_metric_name,
                                            factored_histogram,
                                            event_aliases[i],
                                            histogram);
    }
}

} // namespace perf_streams::event_stream::processor
