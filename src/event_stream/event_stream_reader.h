/*
 * Copyright (c) 2026, Ampere Computing LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "event_stream/event_stream.h"
#include "event_stream/event_stream_proto.h"
#include "protobuf_utils/protobuf_stream.h"

#include <cstdint>
#include <filesystem>
#include <list>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace perf_streams::event_stream {

class EventStreamReader : public protobuf_utils::ProtobufStreamReader
{
public:
    explicit EventStreamReader(int fd);
    explicit EventStreamReader(std::filesystem::path filepath);
    void include(std::string event_name);

    bool read(event_stream_proto::Record& record);
    std::optional<event_stream_proto::Record> read();

    std::optional<event_stream_proto::Event> read_event();
    std::list<event_stream_proto::Definition> all_definitions() const;

    template<typename T>
    std::string name(const T& t) const
    {
        return definitions.at(t.definition_id()).definition.name();
    }

    const auto& definition(EventType definition_id) const { return definitions.at(definition_id).definition; }
    const char* definition_name(EventType definition_id) const
    {
        return definitions.at(definition_id).definition.name().c_str();
    }

    static const char* enumeration_value(const event_stream_proto::Enumeration& enumerations, int64_t value);
    const char* enumeration_value_for_definition(EventType definition_id, int64_t value)
    {
        return enumeration_value_for_definition(definitions.at(definition_id).definition, value);
    }
    const char* enumeration_value_for_definition(const event_stream_proto::Definition& definition, int64_t value)
    {
        if (!definition.has_enumeration_id()) {
            if (auto values = enumerations.find(definition.enumeration_id()); values != enumerations.end())
                return enumeration_value(values->second, value);
        }

        return nullptr;
    }

private:
    void store_definition(const event_stream_proto::Record& record);
    struct DefinitionEntry
    {
        perf_streams::event_stream_proto::Definition definition;
        bool enable{true};
    };
    std::unordered_set<std::string> events_to_include;
    std::unordered_map<EventType, DefinitionEntry> definitions;
    std::unordered_map<EnumType, perf_streams::event_stream_proto::Enumeration> enumerations;
};

} // namespace perf_streams::event_stream
