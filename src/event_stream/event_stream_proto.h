/*
 * Copyright (c) 2026, Ampere Computing LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "event_stream/event.h"
#include "event_stream/event_stream.h"
#include "event_stream/event_stream.pb.h" // IWYU pragma: export
#include "protobuf_utils/protobuf_stream.h"

#include <atomic>
#include <boost/pool/object_pool.hpp>
#include <cstdint>
#include <fmt/base.h>
#include <fmt/ostream.h>
#include <memory>
#include <mutex>
#include <string>

namespace perf_streams::event_stream {

const uint32_t protobuf_magic = 0x53454250; // 0x50(P) 0x42(B) 0x45(E) 0x53(S)
const uint32_t protobuf_es_version = 4;     // synchronize with event_stream.py

class EventStreamProto;

class EventProto : public Event
{
    event_stream_proto::Event event_proto;
    friend class EventStreamProto;
};

class EventStreamProto : public EventStream
{
public:
    explicit EventStreamProto(int fd = 1);
    explicit EventStreamProto(int fd_writer, int fd_reader);
    explicit EventStreamProto(const std::string& filename, bool force = true);

    void post_event(EventType event_type, std::uint64_t time) override;
    EventHandle open_event(EventType event_type, std::uint64_t time) override;
    void close_event(EventHandle event) override;
    void add_int_data(EventHandle event, DataType event_data_type, std::int64_t value) override;
    void add_uint_data(EventHandle event, DataType event_data_type, std::uint64_t value) override;
    void add_string_data(EventHandle event, DataType event_data_type, const std::string& value) override;
    void set_bool_parameter(const std::string& name, const std::string& description, bool value) override;
    void set_int_parameter(const std::string& name, const std::string& description, std::int64_t value) override;
    void set_uint_parameter(const std::string& name, const std::string& description, std::uint64_t value) override;
    void set_double_parameter(const std::string& name, const std::string& description, double value) override;
    void set_string_parameter(const std::string& name,
                              const std::string& description,
                              const std::string& value) override;
    void set_json_parameter(const std::string& name, const std::string& description, const std::string& value) override;
    void start_simulation() override;
    void enable() override { enabled = true; }
    void disable() override { enabled = false; }
    bool is_enabled() override { return enabled; }

private:
    std::unique_ptr<protobuf_utils::ProtobufStreamWriter> writer;
    std::unique_ptr<protobuf_utils::ProtobufStreamReader> reader;
    boost::object_pool<EventProto> event_handle_pool{32, 0};
    std::mutex output_mtx;

    uint32_t num_events{0};

    std::atomic<std::uint64_t> current_event_id;
    bool enabled{true};

    void finalize_event_definition(EventType event_type,
                                   const std::string& name,
                                   const std::string& description) override;
    void finalize_data_definition(DataType data_type, const std::string& name, const std::string& description) override;
    void finalize_data_definition(DataType data_type,
                                  const std::string& name,
                                  const std::string& description,
                                  EnumType enumeration) override;
    void finalize_enum_definition(EnumType enum_type, const EnumMappingType& enumerations) override;

    void write(event_stream_proto::Definition& definition);
    void write(event_stream_proto::Definition& definition, int id, bool& enabled);
    void write(event_stream_proto::Enumeration& enumeration);
    void write(event_stream_proto::Event& event);
    void write(event_stream_proto::Parameter& parameter);
    void write(event_stream_proto::Control& control);
    void write(event_stream_proto::Record& record);
    void write_read(event_stream_proto::Record& record, event_stream_proto::Response& response);
};

} // namespace perf_streams::event_stream

template<>
struct fmt::formatter<perf_streams::event_stream_proto::Kind> : fmt::ostream_formatter
{};
template<>
struct fmt::formatter<perf_streams::event_stream_proto::CtrlType> : fmt::ostream_formatter
{};
template<>
struct fmt::formatter<perf_streams::event_stream_proto::Record::RecordsCase> : fmt::ostream_formatter
{};
