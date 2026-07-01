// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "event_stream/processor/args.h"
#include "event_stream/processor/counter.h"
#include "event_stream/processor/plugin.h"
#include "event_stream/processor/processor.h"
#include "event_stream/processor/processor_ifc.h"
#include "event_stream/processor/utils.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <filesystem>
#include <fmt/base.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <unistd.h>
#include <utility>
#include <vector>

template<>
struct fmt::formatter<std::filesystem::path> : fmt::ostream_formatter
{};

namespace perf_streams::event_stream::processor {

void expand_file_args(std::list<std::string>& args, const std::list<fs::path>& search_paths, const fs::path& from);

/** Read a file and turn the space-separated words into a list of
 *  arguments.
 *
 *  This uses the io manipulator "quote" as an attempt to treat quoted
 *  strings as single arguments. It's useful, but not nearly as potent
 *  as a shell-quote, so this might need improvement.
 */
std::pair<bool, std::list<std::string>> try_rendering_file_as_args(const fs::path& filename,
                                                                   const std::list<fs::path>& search_paths)
{
    if (!fs::exists(filename))
        return {false, {}};

    std::list<std::string> args;
    std::ifstream in{filename.native()};
    std::string line;

    while (std::getline(in, line)) {
        std::istringstream ss(line);

        while (ss) {
            std::string arg;

            ss >> std::quoted(arg);
            if (!arg.empty()) {
                // Any token that begins with char '#' is a comment until the end of the line
                if (arg[0] == '#') {
                    break;
                }

                args.push_back(arg);
            }
        }
    }

    expand_file_args(args, search_paths, filename.parent_path());

    return {true, args};
}

/** Interpolate file contents into command line arguments.
 *
 *  Every command line argument of the form "@name" is potentially a
 *  reference to a file "name" ("name" can be a path). If we can find
 *  that file, either exactly as written or in the config_path, then
 *  we'll read its contents in and insert those arguments into the
 *  given list "args".
 */
void expand_file_args(std::list<std::string>& args, const std::list<fs::path>& search_paths, const fs::path& from)
{
    auto latest_search_paths = search_paths;
    for (auto it = args.begin(); it != args.end();) {
        if (*it == "--config-path" && std::next(it) != args.end()) {
            latest_search_paths.clear();
            latest_search_paths.emplace_back(*++it);
        } else if ((*it)[0] != '@') {
            ++it;
            continue;
        }

        bool expanded = false;
        fs::path argfile = it->substr(1);
        if (auto [found, config_args] = try_rendering_file_as_args(from / argfile, search_paths); found) {
            it = args.erase(it);
            args.splice(it, config_args);
            continue;
        } else if (it->substr(1, 2) != "./" && it->substr(1, 3) != "../") {
            // Only check search paths if non explicitly relative (i.e. does not start with "./")
            for (const auto& config_path : search_paths) {
                auto [found, config_args] = try_rendering_file_as_args(config_path / argfile, search_paths);
                expanded |= found;

                if (found) {
                    it = args.erase(it);
                    args.splice(it, config_args);
                    break;
                }
            }
        }

        if (!expanded)
            throw std::runtime_error{fmt::format("could not find config file {}", argfile)};
    }
}

/** Simple container for a plugin name and argument list.
 */
struct PluginWithArgs
{
    std::string name;
    Args args;

    PluginWithArgs(const std::string& name) : name{name} {}
};

using PluginsWithArgs = std::list<PluginWithArgs>;

struct ArgumentExpansionRef
{
    static constexpr std::string_view open_delimiter = "{{";
    static constexpr size_t open_delimiter_size = open_delimiter.size();
    static constexpr std::string_view close_delimiter = "}}";
    static constexpr size_t close_delimiter_size = close_delimiter.size();
    static constexpr std::string_view parameter_prefix = "param.";
    static constexpr size_t parameter_prefix_size = parameter_prefix.size();
};

/** Partition command line arguments into program arguments and plugin
 *  arguments.
 *
 *  We start by expanding file references into lists of arguments.
 *
 *  Arguments are assumed to be targeted at the main program until we
 *  reach the first plugin name, which will be prefixed with "+". Then
 *  we start parsing arguments as plugin arguments.
 *
 *  So the command line is structured like this, for example:
 *
 *    evp --prog_arg1 prog_arg2                         \
 *        +plugin1 plugin1_arg1 --plugin1_arg2          \
 *        +plugin2                                      \
 *        +plugin3 -plugin3_arg1
 */
std::tuple<Args, PluginsWithArgs> partition_args(int argc, const char** argv)
{
    std::list<std::string> args(&argv[1], &argv[argc]);
    std::list<fs::path> const config_search_paths = {get_config_path()};
    expand_file_args(args, config_search_paths, ".");

    Args prog_args;
    PluginsWithArgs plugins;
    PluginWithArgs* in_plugin = nullptr;

    while (!args.empty()) {
        auto arg = args.front();

        if (arg[0] == '+' || arg == "-P" || arg == "--plugin") {
            std::string name;
            if (arg[0] == '+') {
                name = arg.substr(1);
            } else {
                args.pop_front();
                if (args.empty())
                    throw std::runtime_error{fmt::format("missing plugin name after \"{}\"", arg)};
                name = args.front();
            }

            in_plugin = nullptr;
            if (auto p = registered_plugins().find(name); p != registered_plugins().end() && p->second.signular()) {
                auto existing = std::ranges::find_if(plugins, [&](const auto& p) { return p.name == name; });

                if (existing != plugins.end())
                    in_plugin = &*existing;
            }

            if (!in_plugin)
                in_plugin = &plugins.emplace_back(name);
        } else if (in_plugin) {
            if (Plugin::is_help(arg)) {
                if (auto p = registered_plugins().find(in_plugin->name); p != registered_plugins().end())
                    p->second.help(argc, argv);
            }

            in_plugin->args.push(arg);
        } else {
            prog_args.push(arg);
        }

        args.pop_front();
    }

    return {prog_args, plugins};
}

std::string parameter_as_cli_literal(const Parameter& parameter)
{
    switch (parameter.value_case()) {
    case event_stream_proto::Parameter::kBoolValue:
        return parameter.bool_value() ? "true" : "false";
    case event_stream_proto::Parameter::kIntValue:
        return fmt::format("{}", parameter.int_value());
    case event_stream_proto::Parameter::kUintValue:
        return fmt::format("{}", parameter.uint_value());
    case event_stream_proto::Parameter::kDoubleValue:
        return fmt::format("{}", parameter.double_value());
    case event_stream_proto::Parameter::kStringValue:
        return parameter.string_value();
    case event_stream_proto::Parameter::kJsonValue:
        return parameter.json_value();
    case event_stream_proto::Parameter::VALUE_NOT_SET:
        break;
    }

    throw std::runtime_error{fmt::format("parameter \"{}\" has no value", parameter.name())};
}

std::string trim_whitespace(std::string s)
{
    auto const first = s.find_first_not_of(" \t\n\r\f\v");
    if (first == std::string::npos)
        return {};

    auto const last = s.find_last_not_of(" \t\n\r\f\v");
    return s.substr(first, last - first + 1);
}

std::string resolve_parameter_ref(const std::string& name, const Processor& processor)
{
    if (name.empty())
        throw std::runtime_error{"parameter name is empty in argument expansion"};
    if (!processor.has_parameter(name))
        throw std::runtime_error{fmt::format("parameter \"{}\" not found for argument expansion", name)};

    return parameter_as_cli_literal(processor.get_parameter(name));
}

template<typename Resolve>
std::string expand_argument_refs(const std::string& arg, Resolve resolve)
{
    std::string expanded;

    for (size_t pos = 0; pos < arg.size();) {
        auto const open = arg.find(ArgumentExpansionRef::open_delimiter, pos);
        auto const close_before_open = arg.find(ArgumentExpansionRef::close_delimiter, pos);
        if (close_before_open != std::string::npos && (open == std::string::npos || close_before_open < open))
            throw std::runtime_error{
                fmt::format("unmatched \"{}\" in argument expansion", ArgumentExpansionRef::close_delimiter)};

        if (open == std::string::npos) {
            expanded.append(arg, pos, std::string::npos);
            break;
        }

        auto const close =
            arg.find(ArgumentExpansionRef::close_delimiter, open + ArgumentExpansionRef::open_delimiter_size);
        if (close == std::string::npos)
            throw std::runtime_error{
                fmt::format("unmatched \"{}\" in argument expansion", ArgumentExpansionRef::open_delimiter)};

        expanded.append(arg, pos, open - pos);
        auto const name = trim_whitespace(arg.substr(open + ArgumentExpansionRef::open_delimiter_size,
                                                     close - open - ArgumentExpansionRef::open_delimiter_size));
        if (name.empty())
            throw std::runtime_error{"empty argument expansion"};

        expanded += resolve(name);
        pos = close + ArgumentExpansionRef::close_delimiter_size;
    }

    return expanded;
}

std::string resolve_arg_ref(const std::string& name, const std::map<std::string, std::string>& variables)
{
    if (auto it = variables.find(name); it != variables.end())
        return it->second;

    throw std::runtime_error{fmt::format("argument \"{}\" not found for argument expansion", name)};
}

template<typename ResolveArgument>
std::string resolve_expansion_ref(const std::string& name, const Processor& processor, ResolveArgument resolve_func)
{
    if (name.starts_with(ArgumentExpansionRef::parameter_prefix))
        return resolve_parameter_ref(name.substr(ArgumentExpansionRef::parameter_prefix_size), processor);

    return resolve_func(name);
}

std::string expand_cli_arg_refs(const std::string& arg,
                                const Processor& processor,
                                const std::map<std::string, std::string>& variables)
{
    return expand_argument_refs(arg, [&](const std::string& name) {
        return resolve_expansion_ref(name, processor, [&](const std::string& variable_name) {
            return resolve_arg_ref(variable_name, variables);
        });
    });
}

std::map<std::string, std::string> expand_variables(const std::map<std::string, std::string>& variables,
                                                    const Processor& processor)
{
    std::map<std::string, std::string> expanded_variables;
    std::vector<std::string> expanding;
    std::function<std::string(const std::string&)> expand_variable;

    expand_variable = [&](const std::string& name) {
        if (auto it = expanded_variables.find(name); it != expanded_variables.end())
            return it->second;
        if (std::ranges::find(expanding, name) != expanding.end())
            throw std::runtime_error{fmt::format("cycle detected while expanding argument \"{}\"", name)};
        if (!variables.contains(name))
            return resolve_arg_ref(name, variables);

        expanding.push_back(name);
        auto const expanded = expand_argument_refs(variables.at(name), [&](const std::string& ref_name) {
            return resolve_expansion_ref(ref_name, processor, expand_variable);
        });
        expanding.pop_back();

        expanded_variables[name] = expanded;
        return expanded;
    };

    for (const auto& name : std::views::keys(variables))
        expand_variable(name);

    return expanded_variables;
}

void expand_plugin_args(PluginsWithArgs& plugins,
                        const Processor& processor,
                        const std::map<std::string, std::string>& variables)
{
    for (auto& plugin : plugins) {
        plugin.args.transform([&](const std::string& arg) { return expand_cli_arg_refs(arg, processor, variables); });
    }
}

/** Create a new plugin with associated arguments.
 *
 *  The plugin must have previously registered itself in the global
 *  registry.
 */
void build_plugin(Processor& processor, PluginWithArgs& plugin)
{
    if (auto p = registered_plugins().find(plugin.name); p != registered_plugins().end()) {
        try {
            processor.add_plugin(p->second.factory(processor, plugin.args));
        } catch (...) {
            std::throw_with_nested(std::runtime_error{fmt::format("issue constructing plugin \"{}\"", plugin.name)});
        }
    } else
        throw std::runtime_error{fmt::format("could not find plugin \"{}\"", plugin.name)};
}

/** Build all plugins.
 */
void build_plugins(Processor& processor, PluginsWithArgs& plugins)
{
    for (auto& plugin : plugins)
        build_plugin(processor, plugin);
}

/** This is a silly wrapper around a C file descriptor so that we can
 *  ensure it gets closed when the object is destroyed.
 */
struct CFile
{
    int fd{0};

    ~CFile()
    {
        if (fd > 0)
            close(fd);
    }

    void open_file(const std::string& fname)
    {
        fd = open(fname.c_str(), O_RDONLY);

        if (fd < 0)
            throw std::runtime_error{fmt::format("{}: {}", fname, strerror(fd))};
    }
};

void dump(const Args& args, const PluginsWithArgs& plugins)
{
    if (args.begin() != args.end())
        fmt::print("global options:\n  {}\n\n", fmt::join(args.begin(), args.end(), " "));

    std::map<std::string, unsigned> instances;
    for (const auto& plugin : plugins) {
        fmt::print("{}[{}] plugin options:\n  ", plugin.name, instances[plugin.name]++);
        bool as_value = false;
        size_t i = 0;
        for (const auto& arg : plugin.args) {
            auto is_arg = arg[0] == '-';
            std::cout << (!is_arg && as_value ? " " : ((i++) ? "\n  " : "")) << arg;
            as_value = is_arg;
        }
        std::cout << "\n\n";
    }

    std::cout << '\n';

    exit(EXIT_FAILURE);
}

void help(const char* prog)
{
    constexpr const char* usage = R"(Usage:

    {} [global args...] [plugin args...] [plugin args...]...

Where global args are:

    --es FILENAME.es     Input event stream file. Default stdin
    --in_fd NUM          Input file descriptor.
    --out_fd NUM         Output file descriptor.
    -i INTERVAL          Collect metrics periodically on a time (10us) or metric
                         count interval (see docs for details).
    --start <trigger>    Time or data_value of when to start processing.
    --stop <trigger>     Time or data_value of when to stop processing.
    --exit               Exit evp after stop is reached
    -s <name>=<value>    Set a variable to a value (variables are accessible to
                         all plugins).
    --config-path        Set config file path (overrides subsequent '@' usages)
    --python-path        Set python (plugin) file path (overrides subsequent +python usages)
    --dump               Dump plugins and their arguments
    --help, -h           This help message.

Plugin arguments are introduced with a plugin name followed by arguments
specific to that plugin. E.g.:

    -P|--plugin <plugin name> [<plugin arg1> <plugin arg2>...]

or

    +<plugin name> [<plugin arg1> <plugin arg2>...]

Groups of arguments may also be supplied by proving a file @knobs.cfg;
See the documentation for details of how that file is located.

Available Plugins:

)";

    fmt::print(usage, prog);
    for (const auto& [name, plugin_def] : registered_plugins())
        fmt::print("    {:<12} {:<12} {}\n", name, plugin_def.category(), plugin_def.description);
    std::cout << '\n';

    exit(EXIT_FAILURE);
}

inline void print_exception(std::ostream& os, const std::exception& e, unsigned level = 0)
{
    os << std::string(level * 2U, ' ');
    if (!level)
        os << "Error: ";

    os << e.what() << '\n';
    try {
        std::rethrow_if_nested(e);
    } catch (const std::exception& nested) {
        print_exception(os, nested, level + 1);
    } catch (...) {
    }
}

} // namespace perf_streams::event_stream::processor

using namespace perf_streams::event_stream::processor;

int main(int argc, const char** argv)
{
    try {
        set_config_path();
        auto [args, plugins] = partition_args(argc, argv);

        CFile const input;
        int in_fd{0}, out_fd{0};
        std::string arg;
        std::string input_es;
        std::string interval;
        std::map<std::string, std::string> variables;
        std::string start;
        std::string stop;
        bool exit_after_stop = false;

        while (args) {
            std::string var_setting;

            if (args.pop("--es", input_es)) {
                ;
            } else if (args.pop("--in_fd", in_fd)) {
                ;
            } else if (args.pop("--out_fd", out_fd)) {
                ;
            } else if (args.pop("-i", interval)) {
                ;
            } else if (args.pop("--start", start)) {
                ;
            } else if (args.pop("--stop", stop)) {
                ;
            } else if (args.pop("--exit")) {
                exit_after_stop = true;
            } else if (args.pop("--python-path", arg)) {
                set_pylib_path(arg);
            } else if (args.pop("--dump")) {
                dump(args, plugins);
            } else if (args.pop("--help") || args.pop("-h")) {
                help(std::filesystem::path(argv[0]).filename().c_str());
            } else if (args.pop("-s", var_setting)) {
                size_t const pos = var_setting.find('=');
                if (pos == std::string::npos)
                    throw std::runtime_error(
                        fmt::format("variable setting should be <name>=<value> (got \"{}\")", var_setting));
                auto var = var_setting.substr(0, pos);
                auto value = var_setting.substr(pos + 1);
                if (var.empty())
                    throw std::runtime_error{
                        fmt::format("variable setting should be <name>=<value> (got \"{}\")", var_setting)};
                if (var.starts_with(ArgumentExpansionRef::parameter_prefix))
                    throw std::runtime_error{fmt::format(R"(argument name "{}" cannot start with reserved prefix "{}")",
                                                         var,
                                                         ArgumentExpansionRef::parameter_prefix)};
                variables[var] = value;
            } else {
                break;
            }
        }

        args.done();

        std::unique_ptr<Processor> processor;

        if (!input_es.empty())
            processor = std::make_unique<Processor>(input_es);
        else
            processor = std::make_unique<Processor>(in_fd, out_fd);

        processor->initialize();

        variables = expand_variables(variables, *processor);
        interval = expand_cli_arg_refs(interval, *processor, variables);
        start = expand_cli_arg_refs(start, *processor, variables);
        stop = expand_cli_arg_refs(stop, *processor, variables);
        processor->set_variables(variables);
        expand_plugin_args(plugins, *processor, variables);

        CounterSet skip_counters;
        uint64_t skip_time = 0;
        if (!start.empty()) {
            if (is_time_spec(start)) {
                skip_time = parse_time_spec(start);
                if (skip_time == 0)
                    start.clear();
            } else {
                Action const stop_skip = [&](Counter* counter, const Event& event) {
                    processor->stop_skipping();
                };
                processor->on(nullptr, start, stop_skip, &skip_counters);
                if (auto counter = skip_counters.begin();
                    counter != skip_counters.end() && processor->get_counter(*counter).get_trip() == 0)
                    start.clear();
            }
        }
        if (!stop.empty()) {
            processor->at_or_on(
                nullptr,
                stop,
                [&, exit_after_stop] { processor->stop_at_previous_time(exit_after_stop); },
                &skip_counters);
        }

        build_plugins(*processor, plugins);

        if (!interval.empty()) {
            if (is_time_spec(interval)) {
                TimeAction const collect = [&](uint64_t current_time, uint64_t expiry) {
                    processor->collect(expiry);
                };
                processor->at_every(nullptr, interval, collect);
            } else {
                Action const collect = [&](Counter* counter, const Event& event) {
                    processor->collect();
                };
                processor->on_every(nullptr, interval, collect);
            }
        }

        processor->start();

        if (!start.empty())
            processor->skip(skip_time, &skip_counters);

        processor->process();
        processor->report();

        if (!exit_after_stop)
            processor->skip_remaining();
    } catch (const std::runtime_error& err) {
        print_exception(std::cerr, err);
        exit(EXIT_FAILURE);
    }

    return 0;
}
