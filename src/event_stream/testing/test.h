/*
 * Copyright (c) 2026, Ampere Computing LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "event_stream/event_definition.h"
#include "event_stream/event_stream.h"
#include "event_stream/testing/event_stream_dummy.h"
#include "event_stream/testing/event_stream_mock.h"

#include <memory>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::Return;

namespace perf_streams::event_stream::testing {

struct EventDefinitionExample
{
    EventDefinitionExample(EventStream& event_stream, const std::string& name) : event(event_stream, name, "an event")
    {
    }
    EventDefinition event;
};

template<typename T>
struct EventDataDefinitionExample
{
    template<typename... Args>
    EventDataDefinitionExample(EventStream& event_stream, const std::string& name, Args&&... args)
        : data(event_stream, name, "some data", std::forward<Args>(args)...)
    {
    }
    EventDataDefinition<T> data;
};

template<typename T>
struct BothDefinitionExample : EventDefinitionExample, EventDataDefinitionExample<T>
{
    template<typename... Args>
    explicit BothDefinitionExample(EventStream& event_stream, Args&&... args)
        : EventDefinitionExample(event_stream, "event"), EventDataDefinitionExample<T>(event_stream, "data")
    {
    }
};

struct EventStreamTest : public ::testing::Test
{
    EventStreamTest()
        : event_announcer(std::make_unique<MockEventAnnouncer>()),
          event_stream(std::make_unique<EventStreamDummy>(event_announcer.get()))
    {
    }

    void SetUp() override { event_stream->reset(); }

    std::unique_ptr<MockEventAnnouncer> event_announcer;
    std::unique_ptr<EventStreamDummy> event_stream;
};

struct EventTest : public EventStreamTest
{
    void SetUp() override
    {
        EventStreamTest::SetUp();
        events = std::make_unique<EventDefinitionExample>(*event_stream, "event");
    }

    void TearDown() override { events.reset(); }

    std::unique_ptr<EventDefinitionExample> events;
};

struct EventTestWithMultipleData : public EventStreamTest
{
    struct EventTest
    {
        explicit EventTest(EventStream& event_stream)
            : event{event_stream, "event", "an event"},
              idata{event_stream, "idata", "some integer data"},
              sdata{event_stream, "sdata", "some string data"}
        {
        }
        EventDefinition event;
        EventDataDefinition<int> idata;
        EventDataDefinition<std::string> sdata;
    };

    void SetUp() override
    {
        EventStreamTest::SetUp();
        events = std::make_unique<EventTest>(*event_stream);
    }

    void TearDown() override { events.reset(); }

    std::unique_ptr<EventTest> events;
};

template<typename T>
struct EventTestWithDataBase : public ::testing::TestWithParam<T>
{
    using EventExample = BothDefinitionExample<T>;

    EventTestWithDataBase()
        : event_announcer(std::make_unique<MockEventAnnouncer>()),
          event_stream(std::make_unique<EventStreamDummy>(event_announcer.get()))
    {
    }

    void TearDown() override { events.reset(); }

    std::unique_ptr<MockEventAnnouncer> event_announcer;
    std::unique_ptr<EventStreamDummy> event_stream;
    std::unique_ptr<EventExample> events;
};

} // namespace perf_streams::event_stream::testing
