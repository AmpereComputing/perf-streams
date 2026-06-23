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
    event_stream_latency,
    event_stream_params,
    event_stream_rate,
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
        txid = writer.define_data("txid", "Transaction ID")
        state_enum = writer.define_enumeration({1: "MISS", 2: "HIT"})
        state = writer.define_data("state", "State", enumeration=state_enum)
        writer.set_parameter("width", 4)
        writer.start_simulation()
        writer.post_event(one, time=10, values={value: 7, txid: 1, state: 1})
        writer.post_event(two, time=20, values={txid: 1})
        writer.post_event(one, time=30, values={value: 5, txid: 2, state: 2})
        writer.close()

    def tearDown(self):
        self.work.cleanup()
        event_stream_cache("clear")

    def test_inspect_and_count(self):
        inspected = event_stream_inspect(self.path)
        self.assertEqual(inspected["event_count"], 3)
        self.assertEqual(inspected["counts_by_definition"]["one"], 2)
        self.assertEqual(inspected["enumerations"], [{"id": 0, "values": {1: "MISS", 2: "HIT"}}])
        self.assertNotIn("enumeration_id", inspected["definitions"][3])

        by_name = event_stream_inspect(self.path, name_filter="VAL")
        self.assertEqual([definition["name"] for definition in by_name["definitions"]], ["value"])

        by_description = event_stream_inspect(self.path, description_filter="tw")
        self.assertEqual([definition["name"] for definition in by_description["definitions"]], ["two"])
        self.assertEqual(by_description["counts_by_definition"], {"two": 1})

        counted = event_stream_count(self.path, events=["one"], accumulate=["one/value"])
        self.assertEqual(counted["one"], 2)
        self.assertEqual(counted["one/value"], 12)
        self.assertNotIn("summary", counted)

        interval_counted = event_stream_count(self.path, events=["one"], interval=20)
        self.assertEqual(interval_counted["one"], 2)
        self.assertEqual(interval_counted["series.10.30.one"], 1)
        self.assertEqual(interval_counted["series.30.50.one"], 1)

    def test_rate_and_latency_return_metric_dictionaries(self):
        latency = event_stream_latency(self.path, events=["one", "two"], key="txid")
        self.assertEqual(latency["one_two.count"], 1)
        self.assertEqual(latency["one_two.sum_latency"], 10)
        self.assertNotIn("summary", latency)

        rate = event_stream_rate(self.path, interval="one", events="two")
        self.assertEqual(rate["two.rate.0"], 1)
        self.assertEqual(rate["two.rate.1"], 1)
        self.assertNotIn("summary", rate)

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
