// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "event_stream/event_definition.h"
#include "event_stream/event_stream.h"
#include "event_stream/event_stream_sink.h"
#include "event_stream/testing/event_stream_dummy.h"
#include "event_stream/testing/event_stream_mock.h"
#include "event_stream/testing/test.h"
#include "event_stream/transactional_event_stream.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using perf_streams::event_stream::TransactionalEventStream;
using perf_streams::event_stream::testing::EventTest;

struct TransactionTest : public EventTest
{
    TransactionTest() : transactional_stream(*event_stream) {}
    TransactionalEventStream transactional_stream;

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
