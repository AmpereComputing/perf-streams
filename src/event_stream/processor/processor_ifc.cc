// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "event_stream/processor/processor_ifc.h"

#include "event_stream/event_stream_reader.h"

#include <cstdint>

namespace perf_streams::event_stream::processor {

const char* ProcessorIfc::enumeration_value(int definition_id, int64_t value) const
{
    return enumeration_value_for_definition(get_definition(definition_id), value);
}

const char* ProcessorIfc::enumeration_value_for_definition(const Definition& definition, int64_t value) const
{
    if (has_enumeration(definition)) {
        const auto& enumeration = get_enumeration(definition.enumeration_id());
        return event_stream::EventStreamReader::enumeration_value(enumeration, value);
    }

    return nullptr;
}

} // namespace perf_streams::event_stream::processor
