// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "testing/run_tool.h"

#include <string>

#include <gtest/gtest.h>

struct EventStreamViewerTest : perf_streams::testing::CommandTest
{
    EventStreamViewerTest() : CommandTest(EVENT_STREAM_VIEWER_BIN, "streams") {}
};

TEST_F(EventStreamViewerTest, View)
{
    auto [output, status] = run_command(fixture("four_events_with_data.es"));
    const auto* expected = R"(version=4
definition name=foo description=event id=1
definition name=bar description=data id=2
definition name=baz description=data id=3
definition name=boo description=event id=4
control type=START_SIMULATION
event time=200 name=foo bar=1234 baz=5678
event time=400 name=boo bar=9999
event time=400 name=foo
event time=600 name=boo baz=1
)";
    EXPECT_EQ(status, 0);
    EXPECT_EQ(output, expected);
}

TEST_F(EventStreamViewerTest, ViewAsHex)
{
    auto [output, status] = run_command("-h", fixture("four_events_with_data.es"));
    const auto* expected = R"(version=4
definition name=foo description=event id=1
definition name=bar description=data id=2
definition name=baz description=data id=3
definition name=boo description=event id=4
control type=START_SIMULATION
event time=200 name=foo bar=0x4d2 baz=0x162e
event time=400 name=boo bar=0x270f
event time=400 name=foo
event time=600 name=boo baz=0x1
)";
    EXPECT_EQ(status, 0);
    EXPECT_EQ(output, expected);
}
