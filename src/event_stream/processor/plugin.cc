// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "plugin.h"

#include "event_stream/processor/utils.h"

namespace perf_streams::event_stream::processor {

void Plugin::print_help(const char* prog, const char* name, const char* arguments, const char* details)
{
    constexpr const char* usage = R"(Usage:

    {} +{} {}...

{}
)";

    fmt::print(usage, prog, name, arguments, details);
    exit(EXIT_FAILURE);
}

/** Obtain the singleton map of registered plugins.
 */
PluginRegistry& registered_plugins()
{
    return Singleton<PluginRegistry>::instance();
}

/** Register a new plugin.
 *
 *  Params:
 *    name: name of the plugin
 *    make_plugin: factory function to construct the plugin.
 */
bool register_plugin(const char* name, PluginDefinition definition)
{
    registered_plugins().emplace(name, definition);
    return true;
}

} // namespace perf_streams::event_stream::processor
