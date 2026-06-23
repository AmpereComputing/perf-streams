// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "event_stream/event_stream.pb.h"
#include "event_stream/processor/args.h"
#include "event_stream/processor/metric_table.h"
#include "event_stream/processor/plugin.h"
#include "event_stream/processor/processor_ifc.h"
#include "event_stream/processor/utils.h"

#include <cstdint>
#include <regex>
#include <set>
#include <string>
#include <vector>

namespace perf_streams::event_stream::processor {

EVP_PLUGIN(Param, "param", "Report parameter values")
{
public:
    Param(ProcessorIfc & proc_ifc, Args & args);

    static void help(int argc, const char** argv);
    static bool singular()
    {
        return true;
    }

    void report_parameter(const Parameter& parameter) override;
    void start_simulation() override;
    void collect(MetricSeries & metrics, uint64_t trigger_time) override;
    std::set<Phase> phases() const override
    {
        return {Phase::PARAMETERS};
    }

private:
    void watch_param(std::string arg);

    bool report_all_params{false};
    bool list_params{false};
    std::set<std::string> params_to_report;
    std::vector<std::regex> re_params_to_report;
    MetricSeries params;
};

Param::Param(ProcessorIfc& proc_ifc, Args& args) : Plugin{proc_ifc}
{
    while (args) {
        std::string arg;

        if (args.pop("-p", arg))
            watch_param(arg);
        else if (args.pop("-a"))
            report_all_params = true;
        else if (args.pop("--list"))
            list_params = true;
        else
            break;
    }

    args.done();
}

void Param::help(int argc, const char** argv)
{
    print_help(argv[0], "param", "[-a] [-p <param>]", R"(Arguments:

    -a                          Report all parameters
    -p <param>                  Report named parameter
    --list                      List all parameters getting reported (and exit)
    --help, -h                  This help message.
)");
}

void Param::watch_param(std::string arg)
{
    bool const is_pattern = processor::is_pattern(arg);
    if (is_pattern) {
        re_params_to_report.emplace_back(glob_to_pattern(arg));
    } else {
        params_to_report.insert(arg);
    }
}

/** Record parameter values
 */
void Param::report_parameter(const Parameter& parameter)
{
    if (!report_all_params && !params_to_report.contains(parameter.name())) {
        bool found_match = false;
        for (auto& param_re : re_params_to_report) {
            if (regex_match(parameter.name(), param_re)) {
                found_match = true;
                break;
            }
        }
        if (!found_match)
            return;
    }

    auto which_value = parameter.value_case();
    switch (which_value) {
    case event_stream_proto::Parameter::kBoolValue:
        params[parameter.name()] = parameter.bool_value() ? 1UL : 0UL;
        break;
    case event_stream_proto::Parameter::kUintValue:
        params[parameter.name()] = parameter.uint_value();
        break;
    case event_stream_proto::Parameter::kIntValue:
        params[parameter.name()] = parameter.int_value();
        break;
    case event_stream_proto::Parameter::kDoubleValue:
        params[parameter.name()] = parameter.double_value();
        break;
    case event_stream_proto::Parameter::kStringValue:
        params[parameter.name()] = parameter.string_value();
        break;
    case event_stream_proto::Parameter::kJsonValue:
        params[parameter.name()] = parameter.json_value();
        break;
    default:
        throw PluginError("Event stream Parameter value with unrecognized type");
    }
}

void Param::start_simulation()
{
    if (!list_params)
        return;

    for (auto& [param, _value] : params) {
        fmt::print("param  {}\n", param);
    }

    exit(0);
}

void Param::collect(MetricSeries& metrics, uint64_t trigger_time)
{
    for (auto& [param, value] : params) {
        metrics[param] = value;
    }
}

} // namespace perf_streams::event_stream::processor
