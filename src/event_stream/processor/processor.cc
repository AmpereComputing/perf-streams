// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "processor.h"

#include "event_stream/event_stream_proto.h"
#include "event_stream/processor/bounds.h"
#include "event_stream/processor/processor_ifc.h"
#include "event_stream/processor/utils.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <regex>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace perf_streams::event_stream::processor {

/** A helper class to parse counter specifications.
 *
 *  The general form of a counter specification is:
 *
 *    <name|pattern>[/<factor>[:<value>]][/<factor>[:<value>]]...[=<trip count>]
 *
 *  If you construct a CounterSpec from a string like this, you'll get all the
 *  pieces broken up conveniently:
 *
 *    event_name    - The original name (including globs)
 *    event_pattern - The name converted into a regex pattern if it had globs
 *    event_re      - The name converted into a regex object if it had globs
 *    is_pattern    - Flag indicating that "event_pattern" and "event_re" are valid
 *    factors       - List of factors, if any
 *    trip          - The trip count, if any
 */
class CounterSpec
{
public:
    CounterSpec(std::string s)
    {
        if (size_t const eq_pos = s.find('='); eq_pos != std::string::npos) {
            trip = std::stol(s.substr(eq_pos + 1));
            s = s.substr(0, eq_pos);
        }

        if (size_t pos = s.find('/'); pos != std::string::npos) {
            event_name = s.substr(0, pos);
            s.erase(0, pos + 1);

            while ((pos = s.find('/')) != std::string::npos) {
                factors.emplace_back(s.substr(0, pos));
                s.erase(0, pos + 1);
            }

            if (!s.empty())
                factors.emplace_back(s);
        } else {
            event_name = s;
        }

        is_pattern = processor::is_pattern(event_name);

        if (is_pattern) {
            event_pattern = glob_to_pattern(event_name);
            event_re = event_pattern;
        }
    }

    struct FactorSpec
    {
        explicit FactorSpec(const std::string& s)
        {
            if (auto [event_name, factor_bounds] = FactorBounds::from_event_spec(s); factor_bounds) {
                name = event_name;
                bounds = std::move(factor_bounds);
            } else if (size_t const pos = s.find(':'); pos != std::string::npos) {
                name = s.substr(0, pos);
                value = s.substr(pos + 1);
            } else {
                name = s;
            }
        }

        std::string name;
        std::optional<std::string> value;
        std::unique_ptr<FactorBounds> bounds;
    };

    std::string event_name;
    std::string event_pattern;
    std::regex event_re;
    std::vector<FactorSpec> factors;
    std::optional<int64_t> trip;
    bool is_pattern;
};

/** Create the event stream processor.
 *
 *  The input file descriptor is required; output file descriptor is
 *  optional.
 */
Processor::Processor(int in_fd, int out_fd, const std::map<std::string, std::string>& variables) : variables{variables}
{
    input_stream = std::make_unique<protobuf_utils::ProtobufStreamReader>(
        in_fd, event_stream::protobuf_magic, event_stream::protobuf_es_version);

    if (out_fd > 0)
        response_stream = std::make_unique<protobuf_utils::ProtobufStreamWriter>(
            out_fd, event_stream::protobuf_magic, event_stream::protobuf_es_version);

    rebuild_variables_for_fmt();
}

/** Create the event stream processor from a file name.
 *
 *  This alternative allows for reading compressed files using the different
 *  construction of `ProtobufStreamReader`.
 */
Processor::Processor(const std::string& fname, const std::map<std::string, std::string>& variables)
    : variables{variables}
{
    input_stream = std::make_unique<protobuf_utils::ProtobufStreamReader>(
        fname, event_stream::protobuf_magic, event_stream::protobuf_es_version);

    rebuild_variables_for_fmt();
}

void Processor::set_variables(const std::map<std::string, std::string>& new_variables)
{
    variables = new_variables;
    rebuild_variables_for_fmt();
}

void Processor::rebuild_variables_for_fmt()
{
    variables_for_fmt.clear();

    for (const auto& [name, value] : variables)
        variables_for_fmt.push_back(fmt::arg(name.c_str(), value));
}

/** Read all parameter and definition records from the input and save them.
 *
 *  After this call, the first non-definition/non-parameter is in "record".
 */
void Processor::initialize()
{
    while (input_stream->read(record)) {
        if (record.has_definition()) {
            if (record.definition().kind() == event_stream_proto::EVENT) {
                events.resize(record.definition().id() + 1);
                events[record.definition().id()].state = EventState::DEFINED;
                save_definition(record.definition());
            } else if (record.definition().kind() == event_stream_proto::VALUE) {
                save_definition(record.definition());
            } else {
                throw std::runtime_error{
                    fmt::format("Unrecognized definition type {} in event stream", record.definition().kind())};
            }
        } else if (record.has_parameter()) {
            save_parameter(record.parameter());
        } else if (record.has_control()) {
            if (record.control().type() == event_stream_proto::START_SIMULATION)
                break;
            else
                throw std::runtime_error{
                    fmt::format("Unrecognized control message {} in event stream", record.control().type())};
        } else if (record.has_event()) {
            // Backward compatibility: for streams that don't have START_SIMULATION, we
            // attempt to progress by deferring events for a while in an attempt to
            // gather up the rest of the definitions.
            deferred_events.push_back(record);

            if (deferred_events.size() > DEFERRAL_LIMIT) {
                record = deferred_events.front();
                deferred_events.pop_front();
                break;
            }
        } else if (record.has_enumeration()) {
            save_enumeration(record.enumeration());
        } else {
            throw std::runtime_error{"Bad record in stream"};
        }

        record.Clear();
    }

    if (!record.has_control() && !record.has_event() && !deferred_events.empty()) {
        record = deferred_events.front();
        deferred_events.pop_front();
    }
}

void Processor::ensure_event_record()
{
    if (record.has_event()) {
        ;
    } else if (record.has_definition()) {
        throw std::runtime_error{fmt::format("definition after event: {}", record.definition().name())};
    } else if (record.has_parameter()) {
        throw std::runtime_error{fmt::format("parameter after event: {}", record.parameter().name())};
    } else {
        throw std::runtime_error{fmt::format("unknown record type {} in stream", record.records_case())};
    }
}

void Processor::start()
{
    announce_definitions();
    report_parameters();
    start_simulation();

    if (record.has_event())
        first_event_time = start_time = record.event().time();
}

/** The main processing loop.
 *
 *  Before we get started, announce all definitions to the
 *  plugins. This is their last chance to create event counters,
 *  because once we "start simulation", we will have disabled all
 *  events that have no counter attached.
 *
 *  Then, we just read all event records until we run out. Each event
 *  is handled, and any time based action that should occur *before*
 *  the most recently read event is run.
 *
 *  At the end, we always run a collection.
 */
void Processor::process()
{
    if (phases.contains(Plugin::Phase::COUNTERS) || phases.contains(Plugin::Phase::EVENTS)
        || phases.contains(Plugin::Phase::TRANSACTIONS))
    {
        do {
            ensure_event_record();
            current_time = record.event().time();

            if (!time_based_actions.empty() && current_time >= time_based_actions.begin()->first) {
                run_time_based_actions();
                if (stopped_at_previous_time)
                    break;
            }

            previous_time = current_time;

            handle_event(record.event());
            record.Clear();
        } while (get_next_record() && !stop_processing);

        if (stopped_at_previous_time)
            --previous_time;
    }

    end_simulation();
    collect();
}

void Processor::skip(uint64_t max_time, CounterSet* including)
{
    skipping = true;
    std::set<int> skip_events;
    std::multimap<int, Counter*> skip_counters;
    if (including) {
        for (auto id : *including) {
            auto& counter = get_counter(id);
            skip_events.emplace(counter.event_definition_id);
            skip_counters.emplace(counter.event_definition_id, &counter);
        }
    }

    do {
        ensure_event_record();

        const auto& event = record.event();
        current_time = event.time();
        if (max_time) {
            if (max_time <= current_time)
                skipping = false;
        }
        if (skipping && skip_events.contains(event.definition_id())) {
            auto counter_range = skip_counters.equal_range(event.definition_id());
            for (auto it = counter_range.first; it != counter_range.second; ++it)
                it->second->increment(event);
        }
        if (phases.contains(Plugin::Phase::TRANSACTIONS))
            update_transaction_tracking(event);

        // reprocess record only if stopped skipping on time
        if (!max_time || skipping)
            record.Clear();
    } while (skipping && get_next_record());

    if (skipping)
        throw std::runtime_error("Skipped all events in stream");

    if (!max_time)
        get_next_record();

    // adjust to end of skipped region
    if (record.has_event())
        first_event_time = start_time = record.event().time();
}

void Processor::stop_skipping()
{
    skipping = false;
}

void Processor::stop_at_previous_time(bool exiting)
{
    stop(exiting ? "exiting on stop" : "");
    stopped_at_previous_time = true;
}

void Processor::skip_remaining()
{
    if (!stop_processing)
        return;

    while (get_next_record()) {}
}

bool Processor::get_next_record()
{
    if (deferred_events.empty()) {
        return input_stream->read(record);
    } else {
        record = deferred_events.front();
        deferred_events.pop_front();
        return true;
    }
}

bool Processor::get_next_event()
{
    if (!get_next_record())
        return false;

    ensure_event_record();
    return true;
}

/** Show event/value/enumeration definitions to all plugins.
 */
void Processor::announce_definitions()
{
    auto& definition_plugins = plugins_by_phase[static_cast<size_t>(Plugin::Phase::DEFINITIONS)];
    for (const auto& def : std::views::values(definitions)) {
        for (auto* pp : definition_plugins) {
            if (def.kind() == event_stream_proto::EVENT)
                pp->define_event(def);
            else
                pp->define_value(def);
        }
    }

    for (const auto& def : std::views::values(enumerations))
        for (auto* pp : definition_plugins)
            pp->define_enumeration(def);
}

/** Show event/value definitions to all plugins.
 */
void Processor::report_parameters()
{
    auto& parameter_plugins = plugins_by_phase[static_cast<size_t>(Plugin::Phase::PARAMETERS)];
    for (const auto& param : std::views::values(parameters))
        for (auto* pp : parameter_plugins)
            pp->report_parameter(param);
}

/** Transition from definitions to events.
 *
 *  This is the point at which we will respond with enabled events if
 *  appropriate. We also tell all the plugins that simulation is
 *  starting in case they have something to prepare.
 */
void Processor::start_simulation()
{
    // Older streams don't have START_SIMULATION, so it's not an error
    // if it doesn't appear.  But we don't expect to respond (send
    // back enables/disables) when we don't see START_SIMULATION,
    // because that message is the marker that indicates the simulator
    // is awaiting responses.
    //
    // So we can have a START_SIMULATION without a response stream
    // (such as when reading from a file), but not the other way
    // around.
    //
    if (record.has_control() && record.control().type() == event_stream_proto::START_SIMULATION) {
        if (response_stream)
            enable_active_events();

        record.Clear();

        // We could be blocked for some time in this read (i.e., the
        // entirety of warm up).  If this is an older stream, we would
        // have blocked already, in initialize.
        //
        if (!input_stream->read(record))
            throw std::runtime_error{"unexpected end of records after START_SIMULATION"};
    } else {
        if (response_stream) {
            // If we made it into this block of code, it most likely means
            // something has begun emitting events before we observed
            // START_SIMULATION. Print the events we 'deferred' in initialize()
            // to help the user debug where they are coming from.
            std::map<int32_t, std::pair<std::string, std::string>> events;
            unsigned early_events = 0;

            while (!deferred_events.empty() && events.size() < 20) {
                early_events++;
                auto event = deferred_events.front().event();
                deferred_events.pop_front();

                // only print each event type once
                if (events.contains(event.definition_id()))
                    continue;

                std::string event_name = fmt::format("{}", event.id());
                std::string event_description = fmt::format("{}", event.id());
                if (definitions.contains(event.definition_id())) {
                    auto& definition = definitions[event.definition_id()];
                    event_name = definition.name();
                    event_description = definition.description();
                }
                events.emplace(std::piecewise_construct,
                               std::forward_as_tuple(event.definition_id()),
                               std::forward_as_tuple(event_name, event_description));
            }

            if (!events.empty()) {
                fmt::print("Events observed before START_SIMULATION (name: description):\n");
                for (const auto& [_id, event_info] : events) {
                    const auto& [event_name, event_description] = event_info;
                    fmt::print("    * {}: {}\n", event_name, event_description);
                }

                throw std::runtime_error{
                    fmt::format("encountered (at least) {} events prior to START_SIMULATION, see above for names; "
                                "cannot properly send responses",
                                early_events)};
            }

            if (!input_stream->read(record))
                throw std::runtime_error{"unexpected end of records prior to START_SIMULATION"};
            else
                throw std::runtime_error{"encountered unexpected records prior to START_SIMULATION"};
        }
    }

    for (auto& pp : plugins)
        pp->start_simulation();
}

/** Run end of simulation actions.
 */
void Processor::end_simulation()
{
    for (auto& pp : plugins)
        pp->end_simulation();
}

/** Save a Definition away in our table.
 *
 *  Definitions are also indexed by name for convenience.
 */
void Processor::save_definition(const Definition& definition)
{
    definitions[definition.id()] = definition;
    definition_index[definition.name()] = definition.id();

    if (definition.kind() == event_stream_proto::EVENT) {
        if (definition.name() == "start_transaction")
            start_transaction_definition_id = definition.id();
        else if (definition.name() == "end_transaction")
            end_transaction_definition_id = definition.id();
    } else if (definition.kind() == event_stream_proto::VALUE) {
        if (definition.name() == "txid")
            txid_definition_id = definition.id();
        else if (definition.name() == "parent")
            parent_definition_id = definition.id();
    }
}

/** Save a Parameter away in our table.
 */
void Processor::save_parameter(const Parameter& parameter)
{
    parameters[parameter.name()] = parameter;
}

/** Save a Enumeration away in our table.
 */
void Processor::save_enumeration(const Enumeration& enumeration)
{
    enumerations[enumeration.id()] = enumeration;
}

void Processor::schedule_time_based_action(uint64_t expiry, TimeActionWrapper action)
{
    time_based_actions.emplace(expiry, std::move(action));
}

/** Execute all the time based actions that should occur before
 *  the current_time.
 *
 *  We also re-schedule any recurring actions. Note that reschedule is
 *  not just adding the delta: time might skip forward quite a bit
 *  between events, so we actually move forward by whatever smallest
 *  multiple of delta puts us past the current time.
 */
void Processor::run_time_based_actions()
{
    for (auto t = time_based_actions.begin(); t != time_based_actions.upper_bound(current_time);
         t = time_based_actions.erase(t))
    {
        auto& [expiry, action_wrapper] = *t;
        auto& [action, delta] = action_wrapper;

        // skip expiry before first-event-time as there should be no events
        if (expiry > first_event_time)
            action(current_time, expiry);

        if (delta) {
            auto next_expiry = ((current_time + delta) / delta) * delta;
            schedule_time_based_action(next_expiry, action_wrapper);
        }
    }
}

/** Handle an event:
 *
 *   1. Increment all associated counters.
 *   2. Pass the event to plugins for their own handling.
 */
void Processor::handle_event(const Event& event)
{
    if (phases.contains(Plugin::Phase::TRANSACTIONS))
        update_transaction_tracking(event);

    if (const auto& e = events[event.definition_id()]; e.state == EventState::ENABLED) {
        for (auto* counter : e.counters)
            counter->increment(event);

        auto& event_plugins = plugins_by_phase[static_cast<size_t>(Plugin::Phase::EVENTS)];
        for (auto* pp : event_plugins)
            pp->process_event(event);
    }
}

std::optional<uint64_t> Processor::event_txid(const Event& event) const
{
    if (!txid_definition_id)
        return {};

    for (const auto& value : event.values()) {
        if (value.definition_id() != *txid_definition_id)
            continue;

        switch (value.values_case()) {
        case event_stream_proto::Value::kUintValue:
            return value.uint_value();
        case event_stream_proto::Value::kIntValue:
            if (value.int_value() >= 0)
                return static_cast<uint64_t>(value.int_value());
            return {};
        default:
            return {};
        }
    }

    return {};
}

std::optional<uint64_t> Processor::transaction_parent(uint64_t txid) const
{
    if (auto parent = transaction_parents.find(txid); parent != transaction_parents.end())
        return parent->second;

    return {};
}

bool Processor::is_ancestor(uint64_t ancestor_txid, uint64_t descendant_txid) const
{
    for (auto parent = transaction_parent(descendant_txid); parent; parent = transaction_parent(*parent)) {
        if (*parent == ancestor_txid)
            return true;
    }

    return false;
}

bool Processor::is_related(uint64_t txid_a, uint64_t txid_b) const
{
    return txid_a == txid_b || is_ancestor(txid_a, txid_b) || is_ancestor(txid_b, txid_a);
}

void Processor::update_transaction_tracking(const Event& event)
{
    if (!start_transaction_definition_id || event.definition_id() != *start_transaction_definition_id)
        return;

    auto txid = event_txid(event);
    if (!txid || !parent_definition_id)
        return;

    for (const auto& value : event.values()) {
        if (value.definition_id() != *parent_definition_id)
            continue;

        switch (value.values_case()) {
        case event_stream_proto::Value::kUintValue:
            transaction_parents[*txid] = value.uint_value();
            return;
        case event_stream_proto::Value::kIntValue:
            if (value.int_value() >= 0)
                transaction_parents[*txid] = static_cast<uint64_t>(value.int_value());
            return;
        default:
            return;
        }
    }
}

/** Collect metrics.
 *
 *  For time based collection (time series), we allow a "trigger_time"
 *  to be given. This is because the trigger point might actually lie
 *  *between* the times of two events, but we'd like it to show up in
 *  the time series as the actual desired time---that way if you ask
 *  for metrics every, say, 1us, the start and stop times will be
 *  neatly lined up on microsecond boundaries. But of course we can't
 *  just accept *any* old time---it better lie between the previous
 *  and current event times, inclusively.
 *
 *  If you pass 0 (the default), then you just get collection at the
 *  current time.
 */
void Processor::collect(uint64_t trigger_time)
{
    if (trigger_time == 0)
        trigger_time = current_time;

    if (trigger_time < previous_time || trigger_time > current_time)
        throw std::runtime_error{"collection trigger time is outside of allowed window"};

    MetricSeries series;
    uint64_t const ttime = trigger_time > previous_time ? trigger_time - 1 : trigger_time;

    for (auto& pp : plugins)
        pp->collect(series, ttime);

    ts.times.push_back(ttime);
    ts.add_row(series);

    start_time = trigger_time;
}

/** Trigger all plugins to report.
 *
 *  Typically this would be done after processing is complete.
 */
void Processor::report()
{
    for (auto& pp : plugins)
        pp->report(ts);
}

/** Respond to the simulator with enabled events.
 *
 *  One response is sent per event definition we received. Flush
 *  afterwards because the simulator won't proceed until it has read
 *  that proper number of responses.
 */
void Processor::enable_active_events()
{
    for (auto [event_id, event] : std::views::enumerate(events)) {
        if (event.state == EventState::UNDEFINED)
            continue;

        event_stream_proto::DefinitionResponse definition_response;
        event_stream_proto::Response response;
        definition_response.set_id(event_id);
        definition_response.set_enable(event.state == EventState::ENABLED
                                       || should_enable_event_for_transactions(event_id));
        response.set_allocated_definition(&definition_response);
        response_stream->write(response);
        static_cast<void>(response.release_definition());
    }

    response_stream->flush();
}

bool Processor::should_enable_event_for_transactions(uint32_t event_id) const
{
    return phases.contains(Plugin::Phase::TRANSACTIONS)
           && ((start_transaction_definition_id && event_id == *start_transaction_definition_id)
               || (end_transaction_definition_id && event_id == *end_transaction_definition_id));
}

/** Called by a plugin to count a particular (possibly factored)
 *  event.
 *
 *  If counter_set is non-null, we'll fill in the id of all the
 *  counters that get created, so that the plugin can access those
 *  counters later on.
 */
void Processor::count(Plugin* plugin, const std::string& counter_spec, CounterSet* counter_set, CountAction action)
{
    build_counters(counter_spec, action, counter_set);
}

/** Called by a plugin to trigger an action on a counter.
 *
 *  If there is no trip count ("=<number>") supplied, then we assume
 *  the action is to be called on every occurrence of the event. This
 *  makes it easy for plugins to just call "on" for either kind of
 *  action.
 */
void Processor::on(Plugin* plugin, const std::string& trigger, Action action, CounterSet* counter_set)
{
    if (!trigger.contains('='))
        on_every(plugin, trigger, action, counter_set);
    else
        build_counters(trigger, action, counter_set);
}

/** Called by a plugin to trigger a recurring action.
 *
 *  If a trip count is suppled, e.g., "=100", then every 100
 *  occurrences of the event will trigger the action. If no trip count
 *  is supplied, it is equivalent to "=1", meaning call the action
 *  every time the event occurs.
 */
void Processor::on_every(Plugin* plugin, const std::string& trigger, Action action, CounterSet* counter_set)
{
    CounterSpec spec{trigger};

    uint64_t trip_count;

    if (spec.trip.has_value()) {
        trip_count = *spec.trip;
    } else {
        trip_count = 1;
        spec.trip = 1;
    }

    Action const do_action_and_retrigger = [action, trip_count](Counter* counter, const Event& event) {
        action(counter, event);
        counter->set_trip(counter->get_trip() + trip_count);
    };

    build_counters(spec, do_action_and_retrigger, counter_set);
}

/** Called by a plugin to trigger an action at a particular time
 *  (one-shot).
 */
void Processor::at(Plugin* plugin, const std::string& trigger, TimeAction action)
{
    schedule_time_based_action(parse_time_spec(trigger), TimeActionWrapper{action, 0});
}

/** Called by a plugin to trigger an action at a particular time
 *  interval (recurring).
 */
void Processor::at_every(Plugin* plugin, const std::string& trigger, TimeAction action)
{
    auto delta = parse_time_spec(trigger);
    auto next_expiry = ((current_time + delta) / delta) * delta;
    schedule_time_based_action(next_expiry, TimeActionWrapper{action, delta});
}

/** Called by a plugin to accumlate data values from an event.
 *
 */
void Processor::accumulate(Plugin* plugin, const std::string& counter_spec, CounterSet* counter_set)
{
    build_data_counters(counter_spec, CountAction::ADD, counter_set);
}

/** Build Counters from a specification.
 *
 *  Since a specification can be a pattern, it might create any number
 *  of Counters.  Each one we create should insert its id into "counter_set"
 *  if non-null.
 */
void Processor::build_counters(const CounterSpec& spec, Action action, CounterSet* counter_set)
{
    if (spec.is_pattern) {
        for (auto& [def_id, def] : definitions) {
            if (def.kind() == event_stream_proto::EVENT && regex_match(def.name(), spec.event_re))
                build_counter(def_id, def.name(), spec, action, CountAction::ADD, counter_set);
        }
    } else {
        build_counter(
            get_definition(spec.event_name).id(), spec.event_name, spec, action, CountAction::ADD, counter_set);
    }
}

void Processor::build_counters(const CounterSpec& spec, CountAction action, CounterSet* counter_set)
{
    if (spec.is_pattern) {
        for (auto& [def_id, def] : definitions) {
            if (def.kind() == event_stream_proto::EVENT && regex_match(def.name(), spec.event_re))
                build_counter(def_id, def.name(), spec, nullptr, action, counter_set);
        }
    } else {
        build_counter(get_definition(spec.event_name).id(), spec.event_name, spec, nullptr, action, counter_set);
    }
}

void Processor::build_counter(int event_definition_id,
                              const std::string& event_name,
                              const CounterSpec& spec,
                              Action action,
                              CountAction count_action,
                              CounterSet* counter_set)
{
    int const id = build_counter(event_definition_id, event_name, spec, action, count_action);
    if (counter_set) {
        if (count_action == CountAction::ADD)
            counter_set->insert(id);
        else if (id >= 0)
            counter_set->erase(id);
    }
}

/** Create a new Counter.
 *
 *  A Counter needs its action, trip count, and factors added.
 *
 *  Any event that has a counter is "active", so we record that as well.
 *
 *  The id of the new counter is returned so that it can be added to a counter set.
 */
int Processor::build_counter(int event_definition_id,
                             const std::string& event_name,
                             const CounterSpec& spec,
                             Action action,
                             CountAction count_action)
{
    Counter counter(event_definition_id, action, spec.trip);
    add_factors(counter, event_name, spec);

    if (count_action == CountAction::REMOVE)
        return remove_counter(std::move(counter));
    return add_counter(std::move(counter)).first;
}

namespace {

void add_signed_factor_value(FactorValueMatcher& matcher, int definition_id, int64_t value)
{
    if (value >= 0)
        matcher.values.emplace_back(definition_id, static_cast<uint64_t>(value));
    matcher.values.emplace_back(-definition_id, static_cast<uint64_t>(value));
}

void add_unsigned_factor_value(FactorValueMatcher& matcher, int definition_id, uint64_t value)
{
    matcher.values.emplace_back(definition_id, value);
    if (value <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
        matcher.values.emplace_back(-definition_id, value);
}

bool parse_numeric_factor_value(const std::string& text, int definition_id, FactorValueMatcher& matcher)
{
    try {
        size_t parsed = 0;

        if (!text.empty() && text.front() == '-') {
            const auto value = std::stoll(text, &parsed);
            if (parsed != text.size())
                return false;

            add_signed_factor_value(matcher, definition_id, value);
            return true;
        }

        const auto value = std::stoull(text, &parsed);
        if (parsed != text.size())
            return false;

        add_unsigned_factor_value(matcher, definition_id, value);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace

/** Add factors to a Counter.
 *
 *  Factors allow us to separate counts based on event data. E.g.
 *
 *      cache.access/hit ==> count hit=0 and hit=1 separately
 *
 *  The most annoying part of this is that value names might be scoped
 *  globally, or in the same scope as the event. It's really nice to
 *  be able to write:
 *
 *      path.to.event/data
 *
 *  instead of:
 *
 *      path.to.event/path.to.data
 *
 *  especially is the event path is a pattern. So we allow both. First
 *  check the scoped name because it's most likely to be correct. Then,
 *  try the global name. If you want to force a global name, put "." before
 *  the data name, e.g.:
 *
 *      path.to.event/.must_be_global_data
 */
void Processor::add_factors(Counter& counter, const std::string& event_name, const CounterSpec& spec) const
{
    const auto value_matcher_for = [this](const Definition& definition,
                                          const CounterSpec::FactorSpec& factor) -> std::optional<FactorValueMatcher> {
        if (!factor.value)
            return std::nullopt;

        FactorValueMatcher matcher;
        if (parse_numeric_factor_value(*factor.value, definition.id(), matcher))
            return matcher;

        if (definition.has_enumeration_id() && has_enumeration(definition.enumeration_id())) {
            for (const auto& [value, name] : get_enumeration(definition.enumeration_id()).values()) {
                if (name == *factor.value) {
                    add_signed_factor_value(matcher, definition.id(), value);
                    return matcher;
                }
            }
        }

        throw std::runtime_error{fmt::format(
            R"(factor "{}" value "{}" is not numeric and is not an enum name)", factor.name, *factor.value)};
    };

    for (const auto& factor : spec.factors) {
        const auto add_factor = [&](const Definition& definition) {
            counter.add_factor(definition.id(), factor.bounds.get(), value_matcher_for(definition, factor));
        };

        size_t const last_dot = event_name.rfind('.');

        if (last_dot != std::string::npos) {
            auto scoped_name = fmt::format("{}.{}", event_name.substr(0, last_dot), factor.name);

            if (has_definition(scoped_name)) {
                add_factor(get_definition(scoped_name));
                continue;
            }
        }

        if (has_definition(factor.name)) {
            add_factor(get_definition(factor.name));
            continue;
        } else if (factor.name[0] == '.' && has_definition(factor.name.substr(1))) {
            add_factor(get_definition(factor.name.substr(1)));
            continue;
        }

        // Don't be over-zealous with errors if this event came from a pattern.
        // Just assume the user didn't intend to match this factor.
        //
        if (!spec.is_pattern) {
            throw std::runtime_error{
                fmt::format(R"(unknown data item "{}" used in factor for event "{}")", factor.name, event_name)};
        }
    }
}

std::pair<int, Counter*> Processor::add_counter(Counter counter)
{
    if (auto existing = std::ranges::find_if(counters, [&](const auto& c) { return c && *c == counter; });
        existing != counters.end() && !(**existing).has_action())
    {
        return {std::distance(counters.begin(), existing), &**existing};
    }

    int const counter_id = static_cast<int>(counters.size());
    auto* new_counter = &*counters.emplace_back(std::move(counter));
    auto& e = events[new_counter->event_definition_id];
    e.state = EventState::ENABLED;
    e.counters.emplace_back(new_counter);

    return {counter_id, new_counter};
}

int Processor::remove_counter(Counter counter)
{
    auto existing = std::ranges::find_if(counters, [&](const auto& c) { return c && *c == counter; });
    if (existing == counters.end())
        return -1;

    auto* modified_counter = &**existing;
    auto counter_id = std::distance(counters.begin(), existing);
    auto& e = events[modified_counter->event_definition_id];
    std::erase_if(e.counters, [=](auto* c) { return c == modified_counter; });
    if (e.counters.empty())
        events[modified_counter->event_definition_id].state = EventState::DEFINED;
    existing->reset();

    return counter_id;
}

/** Build Data Counters from a specification.
 *
 *  This kind of counter will accumulate integral data values, e.g.:
 *
 *  Event                   Counter
 *  ===============================
 *  my_event(my_data(10)) ->   10
 *  my_event(my_data(20)) ->   30
 *
 *  etc.
 *
 *  The spec looks like <event>/<data>...that is, like an event with
 *  a single factor.
 *
 *  Currently there are no actions allowed for this kind of counter.
 */
void Processor::build_data_counters(const CounterSpec& spec, CountAction action, CounterSet* counter_set)
{
    if (spec.is_pattern) {
        for (auto& [def_id, def] : definitions) {
            if (def.kind() == event_stream_proto::EVENT && regex_match(def.name(), spec.event_re))
                build_data_counter(def_id, def.name(), spec, action, counter_set);
        }
    } else {
        build_data_counter(get_definition(spec.event_name).id(), spec.event_name, spec, action, counter_set);
    }
}

void Processor::build_data_counter(int event_definition_id,
                                   const std::string& event_name,
                                   const CounterSpec& spec,
                                   CountAction action,
                                   CounterSet* counter_set)
{
    auto id = build_data_counter(event_definition_id, event_name, spec, action);
    if (counter_set) {
        if (action == CountAction::ADD)
            counter_set->insert(id);
        else if (id >= 0)
            counter_set->erase(id);
    }
}

/** Build a single Data Counter.
 *
 *  This is a lot like a normal counter except we only allow one factor.
 */
int Processor::build_data_counter(int event_definition_id,
                                  const std::string& event_name,
                                  const CounterSpec& spec,
                                  CountAction action)
{
    int data_definition_id = -1;

    if (spec.factors.size() == 1) {
        const auto& factor = *spec.factors.begin();

        if (factor.value) {
            throw std::runtime_error{
                fmt::format(R"(factor "{}" is not allowed a value in "{}/{}")", factor.name, factor.name, event_name)};
        }

        if (auto last_dot = event_name.rfind('.'); last_dot != std::string::npos) {
            if (auto scoped_name = fmt::format("{}.{}", event_name.substr(0, last_dot), factor.name);
                has_definition(scoped_name))
            {
                data_definition_id = get_definition(scoped_name).id();
            }
        }

        if (data_definition_id == -1 && has_definition(factor.name)) {
            data_definition_id = get_definition(factor.name).id();
        } else if (factor.name[0] == '.' && has_definition(factor.name.substr(1))) {
            data_definition_id = get_definition(factor.name.substr(1)).id();
        }

        if (data_definition_id == -1 && !spec.is_pattern) {
            throw std::runtime_error{
                fmt::format(R"(unknown data item "{}" used in factor for event "{}")", factor.name, event_name)};
        }
    } else {
        throw std::runtime_error{
            fmt::format("malformed data counter; should have exactly one data item: <event>/<data>")};
    }

    Counter counter(event_definition_id, data_definition_id);
    if (action == CountAction::REMOVE)
        return remove_counter(std::move(counter));
    return add_counter(std::move(counter)).first;
}

/**
 * @brief Stop processing if requested by a plugin.
 *
 * @param msg Diagnostic string: why did we stop?
 */
void Processor::stop(const std::string& msg)
{
    if (!msg.empty())
        fmt::print(std::cerr, "Stopping: {}\n", msg);
    stop_processing = true;
}

} // namespace perf_streams::event_stream::processor
