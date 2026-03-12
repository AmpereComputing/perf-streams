// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "event_stream/event_definition.h"
#include "event_stream/event_stream.h"
#include "event_stream/event_stream_sink.h"
#include "event_stream/testing/event_stream_dummy.h"
#include "event_stream/testing/event_stream_mock.h"
#include "event_stream/testing/test.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoDefault;
using ::testing::EndsWith;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

using namespace perf_streams::event_stream;
using namespace perf_streams::event_stream::testing;

enum class EnumForData {
    A,
    B,
    C
};

namespace perf_streams::event_stream {
template<>
std::pair<bool, EnumMappingType> event_data_definition_enumerations<EnumForData>()
{
    return {true,
            {{static_cast<int64_t>(EnumForData::A), "A"},
             {static_cast<int64_t>(EnumForData::B), "B"},
             {static_cast<int64_t>(EnumForData::C), "C"}}};
}
} // namespace perf_streams::event_stream

struct EventDefinitionTest : public ::testing::Test
{
    EventDefinitionTest() : event_stream(std::make_unique<MockEventStream>(true)) {}

    void TearDown() override { event_stream->do_catch_up(); }

    std::unique_ptr<MockEventStream> event_stream;
};

TEST_F(EventDefinitionTest, CreatesEventDefinition)
{
    EXPECT_CALL(*event_stream, finalize_event_definition(_, "event", "an event"));
    EventDefinitionExample event(*event_stream, "event");
}

TEST_F(EventDefinitionTest, CreatesDataDefinition)
{
    EXPECT_CALL(*event_stream, finalize_data_definition(_, "data", "some data"));
    EventDataDefinitionExample<int> data(*event_stream, "data");
}

TEST_F(EventDefinitionTest, CreatesEnumDefinition)
{
    EXPECT_CALL(*event_stream, finalize_data_definition(_, "data", "some data", _));
    EXPECT_CALL(*event_stream,
                finalize_enum_definition(0,
                                         EnumMappingType({
                                             {0, "A"},
                                             {1, "B"},
                                             {2, "C"},
                                         })));
    EventDataDefinitionExample<EnumForData> data(*event_stream, "data");
}

TEST_F(EventTest, RecordEventAtTime)
{
    EXPECT_CALL(*event_announcer, post_event("event", 100));
    events->event.at(100);
}

template<typename T>
struct EventTestWithData : public EventTestWithDataBase<T>
{
    void SetUp() override
    {
        this->event_stream->reset();
        EXPECT_CALL(*this->event_announcer, post_event("start_transaction", _)).WillRepeatedly(Return());
        EXPECT_CALL(*this->event_announcer, post_event("end_transaction", _)).WillRepeatedly(Return());
        this->events = std::make_unique<typename EventTestWithDataBase<T>::EventExample>(*this->event_stream);
    };

    void do_integer_test(const T& param, bool is_unsigned = false)
    {
        EXPECT_CALL(*this->event_announcer, post_event("event", _));

        if (is_unsigned)
            EXPECT_CALL(*this->event_announcer, add_uint_data("event", _, "data", param));
        else
            EXPECT_CALL(*this->event_announcer, add_int_data("event", _, "data", param, _));

        this->events->event.at(0, this->events->data(param));
    }

    void do_string_test(const T& param)
    {
        EXPECT_CALL(*this->event_announcer, post_event("event", _));
        EXPECT_CALL(*this->event_announcer, add_string_data("event", _, "data", param));

        this->events->event.at(0, this->events->data(param));
    }
};

using EventTestWithSignedChar = EventTestWithData<signed char>;
TEST_P(EventTestWithSignedChar, RecordEventWithSignedChar)
{
    do_integer_test(GetParam());
}
INSTANTIATE_TEST_SUITE_P(RecordSignedChar,
                         EventTestWithSignedChar,
                         ::testing::Values(std::numeric_limits<signed char>::min(),
                                           std::numeric_limits<signed char>::max()));

using EventTestWithUnsignedChar = EventTestWithData<unsigned char>;
TEST_P(EventTestWithUnsignedChar, RecordEventWithUnsignedChar)
{
    do_integer_test(GetParam(), true);
}
INSTANTIATE_TEST_SUITE_P(RecordUnsignedChar,
                         EventTestWithUnsignedChar,
                         ::testing::Values(std::numeric_limits<unsigned char>::min(),
                                           std::numeric_limits<unsigned char>::max()));

using EventTestWithShort = EventTestWithData<short>;
TEST_P(EventTestWithShort, RecordEventWithShort)
{
    do_integer_test(GetParam());
}
INSTANTIATE_TEST_SUITE_P(RecordShort,
                         EventTestWithShort,
                         ::testing::Values(std::numeric_limits<short>::min(), std::numeric_limits<short>::max()));

using EventTestWithUnsignedShort = EventTestWithData<unsigned char>;
TEST_P(EventTestWithUnsignedShort, RecordEventWithUnsignedShort)
{
    do_integer_test(GetParam(), true);
}
INSTANTIATE_TEST_SUITE_P(RecordUnsignedShort,
                         EventTestWithUnsignedShort,
                         ::testing::Values(std::numeric_limits<unsigned short>::min(),
                                           std::numeric_limits<unsigned short>::max()));

using EventTestWithInt = EventTestWithData<int>;
TEST_P(EventTestWithInt, RecordEventWithInt)
{
    do_integer_test(GetParam());
}
INSTANTIATE_TEST_SUITE_P(RecordInt,
                         EventTestWithInt,
                         ::testing::Values(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()));

using EventTestWithUnsignedInt = EventTestWithData<unsigned int>;
TEST_P(EventTestWithUnsignedInt, RecordEventWithUnsignedInt)
{
    do_integer_test(GetParam(), true);
}
INSTANTIATE_TEST_SUITE_P(RecordUnsignedInt,
                         EventTestWithUnsignedInt,
                         ::testing::Values(std::numeric_limits<unsigned int>::min(),
                                           std::numeric_limits<unsigned int>::max()));

using EventTestWithLong = EventTestWithData<long>;
TEST_P(EventTestWithLong, RecordEventWithLong)
{
    do_integer_test(GetParam());
}
INSTANTIATE_TEST_SUITE_P(RecordLong,
                         EventTestWithLong,
                         ::testing::Values(std::numeric_limits<long>::min(), std::numeric_limits<long>::max()));

using EventTestWithUnsignedLong = EventTestWithData<unsigned long>;
TEST_P(EventTestWithUnsignedLong, RecordEventWithUnsignedLong)
{
    do_integer_test(GetParam(), true);
}
INSTANTIATE_TEST_SUITE_P(RecordUnsignedLong,
                         EventTestWithUnsignedLong,
                         ::testing::Values(std::numeric_limits<unsigned long>::min(),
                                           std::numeric_limits<unsigned long>::max()));

using EventTestWithLongLong = EventTestWithData<long long>;
TEST_P(EventTestWithLongLong, RecordEventWithLongLong)
{
    do_integer_test(GetParam());
}
INSTANTIATE_TEST_SUITE_P(RecordLongLong,
                         EventTestWithLongLong,
                         ::testing::Values(std::numeric_limits<long long>::min(),
                                           std::numeric_limits<long long>::max()));

using EventTestWithUnsignedLongLong = EventTestWithData<unsigned long long>;
TEST_P(EventTestWithUnsignedLongLong, RecordEventWithUnsignedLongLong)
{
    do_integer_test(GetParam(), true);
}
INSTANTIATE_TEST_SUITE_P(RecordUnsignedLongLong,
                         EventTestWithUnsignedLongLong,
                         ::testing::Values(std::numeric_limits<unsigned long long>::min(),
                                           std::numeric_limits<unsigned long long>::max()));

using EventTestWithCString = EventTestWithData<const char*>;
TEST_P(EventTestWithCString, RecordEventWithCString)
{
    do_string_test(GetParam());
}
INSTANTIATE_TEST_SUITE_P(RecordCString, EventTestWithCString, ::testing::Values("hello world"));

using EventTestWithString = EventTestWithData<std::string>;
TEST_P(EventTestWithString, RecordEventWithString)
{
    do_string_test(GetParam());
}
INSTANTIATE_TEST_SUITE_P(RecordString, EventTestWithString, ::testing::Values(std::string{"hello world"}));

using EventTestWithEnum = EventTestWithData<EnumForData>;
TEST_F(EventTestWithEnum, RecordEventWithEnum)
{
    EXPECT_CALL(*event_announcer, post_event("event", _));
    EXPECT_CALL(*event_announcer, add_int_data("event", _, "data", static_cast<int>(EnumForData::B), _));
    events->event.at(0, events->data(EnumForData::B));
}

TEST_F(EventTestWithMultipleData, RecordWithMultipleData)
{
    EXPECT_CALL(*event_announcer, post_event("event", _));
    EXPECT_CALL(*event_announcer, add_int_data("event", _, "idata", 1000001, _));
    EXPECT_CALL(*event_announcer, add_string_data("event", _, "sdata", "red roses"));

    events->event.at(0, events->idata(1000001), events->sdata("red roses"));
}

TEST_F(EventTestWithMultipleData, RecordWithOptionalData)
{
    EXPECT_CALL(*event_announcer, post_event("event", _));
    EXPECT_CALL(*event_announcer, add_string_data("event", _, "sdata", "red roses"));

    events->event.at(0, events->idata.if_set(0), events->sdata("red roses"));
}

TEST_F(EventTestWithMultipleData, RecordWithPredicatedOptionalData)
{
    EXPECT_CALL(*event_announcer, post_event("event", _));
    EXPECT_CALL(*event_announcer, add_int_data("event", _, "idata", 1000001, _));

    events->event.at(0, events->idata(1000001), events->sdata.if_set("red roses", false));
}

TEST_F(EventTestWithMultipleData, RecordWithMultipleOptionalData)
{
    EXPECT_CALL(*event_announcer, post_event("event", _));
    EXPECT_CALL(*event_announcer, add_int_data("event", _, "idata", 1000001, _));
    EXPECT_CALL(*event_announcer, add_string_data("event", _, "sdata", "red roses"));

    events->event.at(0, events->idata.if_set(1000001), events->sdata.if_set("red roses", true));
}
