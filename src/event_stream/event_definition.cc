// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "event_definition.h"

#include "event_stream/event_stream.h"

#include <string>

namespace perf_streams::event_stream {

EventDefinition::EventDefinition(EventStream& event_stream, const std::string& name, const std::string& description)
    : _name(name), _description(description), event_stream(&event_stream), definition(event_stream.define_event(*this))
{
}

void EventDefinition::at(TimeType at_time)
{
    if (enabled && event_stream->is_enabled())
        event_stream->post_event(definition, at_time);
}

} // namespace perf_streams::event_stream
