// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "transactional_event_stream.h"

#include "event_stream/event.h"
#include "event_stream/event_stream.h"
#include "event_stream/transaction.h"

#include <cstdint>
#include <string>
#include <utility>

namespace perf_streams::event_stream {

EventHandle TransactionalEventStream::open_event(EventType event_type, TimeType time)
{
    return event_stream->open_event(event_type, time);
}
void TransactionalEventStream::close_event(EventHandle event)
{
    event_stream->close_event(event);
}

void TransactionalEventStream::post_event(EventType event_type, TimeType time, Transaction& transaction)
{
    event_stream->close_event(open_event(event_type, time, transaction));
}

EventHandle TransactionalEventStream::open_event(EventType event_type, TimeType time, Transaction& transaction)
{
    auto* event = event_stream->open_event(event_type, time);
    event_stream->add_uint_data(event, definition.txid, transaction.id());
    return event;
}

TransactionHandle TransactionalEventStream::begin_transaction(TimeType time, TransactionHandle parent)
{
    auto [event, transaction] = begin_transaction_with_data(time, parent);
    event_stream->close_event(event);
    return transaction;
}

std::pair<EventHandle, TransactionHandle> TransactionalEventStream::begin_transaction_with_data(
    TimeType time, TransactionHandle parent)
{
    auto* transaction = new Transaction(*this);
    auto* event = event_stream->open_event(definition.start_transaction, time);
    event_stream->add_uint_data(event, definition.txid, transaction->id());
    if (parent)
        event_stream->add_uint_data(event, definition.parent, parent->id());

    return {event, transaction};
}

void TransactionalEventStream::end_transaction(TransactionHandle transaction, TimeType time)
{
    event_stream->close_event(end_transaction_with_data(transaction, time));
}

EventHandle TransactionalEventStream::end_transaction_with_data(TransactionHandle transaction, TimeType time)
{
    auto* event = event_stream->open_event(definition.end_transaction, time);
    event_stream->add_uint_data(event, definition.txid, transaction->id());

    return event;
}

void TransactionalEventStream::add_int_data(EventHandle event, DataType event_data_type, std::int64_t value)
{
    event_stream->add_int_data(event, event_data_type, value);
}
void TransactionalEventStream::add_uint_data(EventHandle event, DataType event_data_type, std::uint64_t value)
{
    event_stream->add_uint_data(event, event_data_type, value);
}
void TransactionalEventStream::add_string_data(EventHandle event, DataType event_data_type, const std::string& value)
{
    event_stream->add_string_data(event, event_data_type, value);
}

void TransactionalEventStream::set_bool_parameter(const std::string& name, const std::string& description, bool value)
{
    event_stream->set_bool_parameter(name, description, value);
}
void TransactionalEventStream::set_int_parameter(const std::string& name,
                                                 const std::string& description,
                                                 std::int64_t value)
{
    event_stream->set_int_parameter(name, description, value);
}
void TransactionalEventStream::set_uint_parameter(const std::string& name,
                                                  const std::string& description,
                                                  std::uint64_t value)
{
    event_stream->set_uint_parameter(name, description, value);
}
void TransactionalEventStream::set_double_parameter(const std::string& name,
                                                    const std::string& description,
                                                    double value)
{
    event_stream->set_double_parameter(name, description, value);
}
void TransactionalEventStream::set_string_parameter(const std::string& name,
                                                    const std::string& description,
                                                    const std::string& value)
{
    event_stream->set_string_parameter(name, description, value);
}
void TransactionalEventStream::set_json_parameter(const std::string& name,
                                                  const std::string& description,
                                                  const std::string& value)
{
    event_stream->set_json_parameter(name, description, value);
}

void TransactionalEventStream::TransactionDefinitions::setup(EventStream* event_stream)
{
    start_transaction = event_stream->define_event("start_transaction", "Beginning of a new transaction");
    end_transaction = event_stream->define_event("end_transaction", "End of a transaction");
    txid = event_stream->define_data("txid", "Transaction ID");
    parent = event_stream->define_data("parent", "ID of parent transaction");
}

void TransactionalEventStream::start_simulation()
{
    definition.setup(event_stream);
    event_stream->start_simulation();
}

void TransactionalEventStream::enable()
{
    event_stream->enable();
}

void TransactionalEventStream::disable()
{
    event_stream->disable();
}

void TransactionalEventStream::finalize_event_definition(EventType event_type,
                                                         const std::string& name,
                                                         const std::string& description)
{
    event_stream->finalize_event_definition(event_type, name, description);
}
void TransactionalEventStream::finalize_data_definition(DataType data_type,
                                                        const std::string& name,
                                                        const std::string& description)
{
    event_stream->finalize_data_definition(data_type, name, description);
}

void TransactionalEventStream::finalize_data_definition(DataType data_type,
                                                        const std::string& name,
                                                        const std::string& description,
                                                        EnumType enumeration [[maybe_unused]])
{
    event_stream->finalize_data_definition(data_type, name, description, enumeration);
}
void TransactionalEventStream::finalize_enum_definition(EnumType enum_type [[maybe_unused]],
                                                        const EnumMappingType& enumerations [[maybe_unused]])
{
    event_stream->finalize_enum_definition(enum_type, enumerations);
}

} // namespace perf_streams::event_stream
