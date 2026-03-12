/*
 * Copyright (c) 2026, Ampere Computing LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "event_stream/event_definition.h"
#include "event_stream/event_stream.h"

#include <cstdint>

namespace perf_streams::event_stream {

class TransactionalEventStream;

class Transaction
{
public:
    explicit Transaction(TransactionalEventStream& event_stream);
    virtual ~Transaction() = default;

    auto id() const { return txid; }

protected:
    DataType txid_definition() const;

    uint64_t txid;
    static uint64_t current_id;
    TransactionalEventStream* event_stream{nullptr};

    friend EventArg<uint64_t> to_event_arg(const Transaction& transaction);
};

EventArg<uint64_t> to_event_arg(const Transaction& transaction);

} // namespace perf_streams::event_stream
