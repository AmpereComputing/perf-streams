// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "event_stream/event_stream_with.h"
#include "event_stream/extensions/transactions.h"
#include "event_stream/testing/test.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using perf_streams::event_stream::EventStreamWith;
using perf_streams::event_stream::testing::EventTest;

namespace extensions = perf_streams::event_stream::extensions;

struct TransactionTest : public EventTest
{
    TransactionTest() : transactional_stream(*event_stream) {}
    EventStreamWith<extensions::Transactions> transactional_stream;

    void SetUp() override
    {
        EventTest::SetUp();
        transactional_stream.start_simulation();
    }
};

TEST_F(TransactionTest, RecordEventWithTransaction)
{
    EXPECT_CALL(*event_announcer, post_event("start_transaction", _));
    EXPECT_CALL(*event_announcer, add_uint_data("start_transaction", _, _, _));
    EXPECT_CALL(*event_announcer, post_event("end_transaction", _));
    EXPECT_CALL(*event_announcer, add_uint_data("end_transaction", _, _, _));
    EXPECT_CALL(*event_announcer, post_event("event", _));
    EXPECT_CALL(*event_announcer, add_uint_data("event", _, "txid", _));

    auto* transaction = transactional_stream.begin_transaction(0);
    events->event.at(0, *transaction);
    transactional_stream.end_transaction(transaction, 0);
}
