// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "event_stream/event_definition.h"
#include "event_stream/event_stream.h"
#include "event_stream/event_stream_sink.h"
#include "event_stream/event_stream_with.h"
#include "event_stream/testing/event_stream_dummy.h"
#include "event_stream/testing/event_stream_mock.h"
#include "event_stream/testing/test.h"
#include "event_stream/extensions/transactions.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using perf_streams::event_stream::EventStreamExtension;
using perf_streams::event_stream::EventStreamLayer;
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

template<EventStreamLayer Base>
class TestExtensionA : public Base
{
public:
    using Base::Base;

    int extension_value() const { return 41; }
};

template<EventStreamLayer Base>
class TestExtensionB : public Base
{
public:
    using Base::Base;

    int dependent_value() const { return this->extension_value() + 1; }
};

namespace perf_streams::event_stream::extensions {

template<>
struct EventStreamExtensionTraits<TestExtensionB>
{
    using requirements = std::tuple<EventStreamExtension<TestExtensionA>>;
};

} // namespace perf_streams::event_stream::extensions

static_assert(perf_streams::event_stream::event_stream_extensions_valid_v<TestExtensionA, TestExtensionB>);
static_assert(!perf_streams::event_stream::event_stream_extensions_valid_v<TestExtensionB>);

TEST_F(TransactionTest, ForwarderUsesWrappedDefinitions)
{
    EventStreamWith<> stream(*event_stream);
    perf_streams::event_stream::EventDefinition event(stream, "wrapped_event", "wrapped event");

    EXPECT_CALL(*event_announcer, post_event("wrapped_event", 7));

    event.at(7);
}

TEST_F(TransactionTest, ExtensionsCanBuildOnEarlierExtensions)
{
    EventStreamWith<TestExtensionA, TestExtensionB> const stream(*event_stream);

    EXPECT_EQ(stream.dependent_value(), 42);
}
