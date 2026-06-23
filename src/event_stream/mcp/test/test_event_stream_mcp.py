# Copyright (c) 2026, Ampere Computing LLC
# SPDX-License-Identifier: BSD-3-Clause

import os
import unittest
from tempfile import TemporaryDirectory

from perf_streams.event_stream import EventStreamWriter
from perf_streams.event_stream_mcp import (
    event_stream_cache,
    event_stream_count,
    event_stream_inspect,
    event_stream_params,
    event_stream_sample,
)


class TestEventStreamMCPTools(unittest.TestCase):
    def setUp(self):
        self.work = TemporaryDirectory()
        self.path = os.path.join(self.work.name, "test.es")

        writer = EventStreamWriter(self.path)
        one = writer.define_event("one", "One")
        two = writer.define_event("two", "Two")
        value = writer.define_data("value", "Value")
        writer.set_parameter("width", 4)
        writer.start_simulation()
        writer.post_event(one, time=10, values={value: 7})
        writer.post_event(two, time=20)
        writer.post_event(one, time=30, values={value: 5})
        writer.close()

    def tearDown(self):
        self.work.cleanup()
        event_stream_cache("clear")

    def test_inspect_and_count(self):
        inspected = event_stream_inspect(self.path)
        self.assertEqual(inspected["event_count"], 3)
        self.assertEqual(inspected["counts_by_definition"]["one"], 2)

        by_name = event_stream_inspect(self.path, name_filter="VAL")
        self.assertEqual([definition["name"] for definition in by_name["definitions"]], ["value"])

        by_description = event_stream_inspect(self.path, description_filter="tw")
        self.assertEqual([definition["name"] for definition in by_description["definitions"]], ["two"])
        self.assertEqual(by_description["counts_by_definition"], {"two": 1})

        counted = event_stream_count(self.path, events=["one"], accumulate=["one/value"])
        metrics = {metric["name"]: metric["value"] for metric in counted["summary"]}
        self.assertEqual(metrics["one"], 2)
        self.assertEqual(metrics["one/value"], 12)

    def test_params_sample_and_cache(self):
        params = event_stream_params(self.path, all=True)
        self.assertEqual(params["parameters"][0]["name"], "width")
        self.assertEqual(params["parameters"][0]["value"], 4)

        sampled = event_stream_sample(self.path, events="one", limit=1)
        self.assertEqual(len(sampled["events"]), 1)
        self.assertEqual(sampled["events"][0]["name"], "one")
        self.assertTrue(sampled["truncated"])

        stats = event_stream_cache("stats")
        self.assertEqual(stats["streams"], 1)


if __name__ == "__main__":
    unittest.main()
