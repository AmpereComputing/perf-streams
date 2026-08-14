// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "transaction.h"

#include "event_stream/event_definition.h"
#include "event_stream/event_stream.h"

#include <cstdint>

namespace perf_streams::event_stream {

uint64_t Transaction::current_id = 0;

Transaction::Transaction(DataType txid_definition) : txid(++current_id), txid_data_type(txid_definition) {}

EventArg<uint64_t> to_event_arg(const Transaction& transaction)
{
    return {transaction.txid_definition(), transaction.id()};
}

} // namespace perf_streams::event_stream
