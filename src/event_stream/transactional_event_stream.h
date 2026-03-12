/*
 * Copyright (c) 2026, Ampere Computing LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "event_stream/event.h"
#include "event_stream/event_stream.h"
#include "event_stream/transaction.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace perf_streams::event_stream {

using TransactionHandle = Transaction*;

class TransactionalEventStream : public EventStream
{
public:
    explicit TransactionalEventStream(EventStream& event_stream) : event_stream(&event_stream) {}
    explicit TransactionalEventStream(std::unique_ptr<EventStream>&& event_stream)
        : owned_event_stream(std::move(event_stream)), event_stream(owned_event_stream.get())
    {
    }

    using EventStream::post_event;
    EventHandle open_event(EventType event_type, TimeType time) override;
    void close_event(EventHandle event) override;

    void post_event(EventType event_type, TimeType time, Transaction& transaction);
    EventHandle open_event(EventType event_type, TimeType time, Transaction& transaction);

    TransactionHandle begin_transaction(TimeType time, TransactionHandle parent = nullptr);
    std::pair<EventHandle, TransactionHandle> begin_transaction_with_data(TimeType time,
                                                                          TransactionHandle parent = nullptr);

    void end_transaction(TransactionHandle transaction, TimeType time);
    EventHandle end_transaction_with_data(TransactionHandle transaction, TimeType time);

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

    void enable() override;
    void disable() override;

protected:
    void finalize_event_definition(EventType event_type,
                                   const std::string& name,
                                   const std::string& description) override;
    void finalize_data_definition(DataType data_type, const std::string& name, const std::string& description) override;
    void finalize_data_definition(DataType data_type,
                                  const std::string& name,
                                  const std::string& description,
                                  EnumType enumeration [[maybe_unused]]) override;
    void finalize_enum_definition(EnumType enum_type [[maybe_unused]],
                                  const EnumMappingType& enumerations [[maybe_unused]]) override;

    std::unique_ptr<EventStream> owned_event_stream;
    EventStream* event_stream{nullptr};

    struct TransactionDefinitions
    {
        EventType start_transaction;
        EventType end_transaction;
        DataType txid;
        DataType parent;

        void setup(EventStream* event_stream);
    } definition;

    friend class Transaction;
};

} // namespace perf_streams::event_stream
