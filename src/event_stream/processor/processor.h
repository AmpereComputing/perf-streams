/*
 * Copyright (c) 2026, Ampere Computing LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "event_stream/event_stream.pb.h"
#include "event_stream/processor/counter.h"
#include "event_stream/processor/metric_table.h"
#include "event_stream/processor/plugin.h"
#include "event_stream/processor/processor_ifc.h"
#include "event_stream/processor/transaction_tracker.h"
#include "event_stream/processor/utils.h"
#include "protobuf_utils/protobuf_stream.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <fmt/args.h>
#include <fmt/base.h>
#include <fmt/core.h>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace perf_streams::event_stream::processor {

class CounterSpec;

class Processor : public ProcessorIfc
{
public:
    Processor(int in_fd, int out_fd, const std::map<std::string, std::string>& variables = {});
    explicit Processor(const std::string& fname, const std::map<std::string, std::string>& variables = {});

    void initialize();
    void start();
    void process();
    void collect(uint64_t trigger_time = 0);
    void report();

    void set_variables(const std::map<std::string, std::string>& new_variables);

    void skip(uint64_t max_time = 0, CounterSet* including = nullptr);
    void stop_skipping();
    void stop_at_previous_time(bool exiting = false);
    void skip_remaining();

    void add_plugin(std::unique_ptr<Plugin> plugin)
    {
        auto& p = plugins.emplace_back(std::move(plugin));
        auto phases = p->phases();
        for (auto phase : phases)
            plugins_by_phase[static_cast<size_t>(phase)].emplace_back(p.get());

        this->phases.merge(phases);
    }

    //
    // ProcessorIfc implementation
    //

    bool has_definition(int id) const override { return definitions.contains(id); }
    bool has_definition(const std::string& name) const override { return definition_index.contains(name); }
    bool has_parameter(const std::string& name) const override { return parameters.contains(name); }
    bool has_enumeration(int id) const override { return enumerations.contains(id); }

    const Definition& get_definition(int id) const override { return definitions.at(id); }

    const Definition& get_definition(const std::string& name) const override
    {
        try {
            return definitions.at(definition_index.at(name));
        } catch (std::out_of_range&) {
            throw NoSuchDefinition{name};
        }
    }

    const Parameter& get_parameter(const std::string& name) const override { return parameters.at(name); }
    const Enumeration& get_enumeration(int id) const override { return enumerations.at(id); }

    Counter& get_counter(int counter_id) override { return counters.at(counter_id).value(); }

    uint64_t get_current_time() const override { return current_time; }
    uint64_t get_first_event_time() const override { return first_event_time; }
    std::optional<uint64_t> event_txid(const Event& event) const override;
    std::optional<uint64_t> transaction_parent(uint64_t txid) const override;
    bool is_ancestor(uint64_t ancestor_txid, uint64_t descendant_txid) const override;
    bool is_related(uint64_t txid_a, uint64_t txid_b) const override;

    void count(Plugin* plugin,
               const std::string& counter_spec,
               CounterSet* counter_set = nullptr,
               CountAction action = CountAction::ADD) override;
    void on(Plugin* plugin, const std::string& trigger, Action action, CounterSet* counter_set = nullptr) override;
    void on_every(Plugin* plugin,
                  const std::string& trigger,
                  Action action,
                  CounterSet* counter_set = nullptr) override;
    void at(Plugin* plugin, const std::string& trigger, TimeAction action) override;
    void at_every(Plugin* plugin, const std::string& trigger, TimeAction action) override;
    void accumulate(Plugin* plugin, const std::string& counter_spec, CounterSet* counter_set = nullptr) override;

    template<typename P>
    void at_or_on(Plugin* plugin, const std::string& trigger, P trip, CounterSet* counter_set = nullptr)
    {
        if (is_time_spec(trigger)) {
            TimeAction const time_action = [=](uint64_t current_time, uint64_t expiry) {
                trip();
            };
            at(plugin, trigger, time_action);
        } else {
            Action const action = [=](Counter* counter, const Event& event) {
                trip();
            };
            on(plugin, trigger, action, counter_set);
        }
    }

    const std::map<std::string, std::string>& vars() override { return variables; }
    fmt::format_args vars_for_fmt() override { return variables_for_fmt; }

    void stop(const std::string& msg) override;

private:
    std::unique_ptr<protobuf_utils::ProtobufStreamReader> input_stream;
    std::unique_ptr<protobuf_utils::ProtobufStreamWriter> response_stream;

    event_stream_proto::Record record;

    const size_t DEFERRAL_LIMIT = 50000;
    std::list<event_stream_proto::Record> deferred_events;

    std::map<int, Definition> definitions;
    std::map<std::string, int> definition_index;
    std::map<std::string, Parameter> parameters;
    std::map<int, Enumeration> enumerations;

    enum class EventState {
        UNDEFINED,
        DEFINED,
        ENABLED
    };
    struct EventCounters
    {
        EventState state{EventState::UNDEFINED};
        std::vector<Counter*> counters;
    };
    std::vector<EventCounters> events;
    std::deque<std::optional<Counter>> counters;

    std::set<Plugin::Phase> phases;
    std::vector<std::unique_ptr<Plugin>> plugins;
    std::array<std::vector<Plugin*>, static_cast<size_t>(Plugin::Phase::SIZE)> plugins_by_phase;

    TransactionTracker transaction_tracker;

    MetricTableTimeSeries ts;

    uint64_t current_time{0};
    uint64_t previous_time{0};
    uint64_t start_time{0};
    uint64_t first_event_time{0};

    bool skipping{false};
    bool stop_processing{false};
    bool stopped_at_previous_time{false};

    struct TimeActionWrapper
    {
        TimeAction action;
        uint64_t delta;

        TimeActionWrapper(TimeAction action, uint64_t delta) : action{action}, delta{delta} {}
    };

    std::multimap<uint64_t, TimeActionWrapper> time_based_actions;

    std::map<std::string, std::string> variables;
    fmt::dynamic_format_arg_store<fmt::format_context> variables_for_fmt;
    void rebuild_variables_for_fmt();

    bool get_next_record();
    bool get_next_event();
    void save_definition(const Definition& definition);
    void save_parameter(const Parameter& parameter);
    void save_enumeration(const Enumeration& enumeration);
    void announce_definitions();
    void report_parameters();
    void start_simulation();
    void end_simulation();
    void schedule_time_based_action(uint64_t expiry, TimeActionWrapper action);
    void run_time_based_actions();
    void ensure_event_record();
    void handle_event(const Event& event);
    void update_transaction_tracking(const Event& event);
    bool should_enable_event_for_transactions(uint32_t event_id) const;

    void enable_active_events();

    void build_counters(const CounterSpec& spec, Action action, CounterSet* counter_set);
    void build_counters(const CounterSpec& spec, CountAction action, CounterSet* counter_set);
    void build_counter(int event_definition_id,
                       const std::string& event_name,
                       const CounterSpec& spec,
                       Action action,
                       CountAction count_action,
                       CounterSet* counter_set);
    int build_counter(int event_definition_id,
                      const std::string& event_name,
                      const CounterSpec& spec,
                      Action action,
                      CountAction count_action = CountAction::ADD);

    std::pair<int, Counter*> add_counter(Counter counter);
    int remove_counter(Counter counter);
    void add_factors(Counter& counter, const std::string& event_name, const CounterSpec& spec) const;

    void build_data_counters(const CounterSpec& spec, CountAction action, CounterSet* counter_set);
    void build_data_counter(int event_definition_id,
                            const std::string& event_name,
                            const CounterSpec& spec,
                            CountAction action,
                            CounterSet* counter_set);
    int build_data_counter(int event_definition_id,
                           const std::string& event_name,
                           const CounterSpec& spec,
                           CountAction action);
};

} // namespace perf_streams::event_stream::processor
