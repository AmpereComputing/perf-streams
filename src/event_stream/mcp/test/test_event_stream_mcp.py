# Copyright (c) 2026, Ampere Computing LLC
# SPDX-License-Identifier: BSD-3-Clause

import inspect
import os
import unittest
from tempfile import TemporaryDirectory

from perf_streams import event_stream_mcp
from perf_streams.event_stream import EventStreamWriter


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
        writer.set_parameter("width", 4, description="Machine Width")
        writer.set_parameter("height", 9, description="Queue Height")
        writer.start_simulation()
        writer.post_event(one, time=10, values={value: 7, txid: 1, state: 1})
        writer.post_event(two, time=20, values={txid: 1})
        writer.post_event(one, time=30, values={value: 5, txid: 2, state: 2})
        writer.close()

    def tearDown(self):
        self.work.cleanup()
        event_stream_mcp.cache("clear")

    def test_inspect_and_count(self):
        inspected = event_stream_mcp.inspect(self.path)
        self.assertEqual(inspected["event_count"], 3)
        self.assertEqual(inspected["counts_by_definition"]["one"], 2)
        self.assertEqual(inspected["enumerations"], [{"id": 0, "values": {1: "MISS", 2: "HIT"}}])
        self.assertNotIn("enumeration_id", inspected["definitions"][3])

        by_name = event_stream_mcp.inspect(self.path, name_filter="VAL")
        self.assertEqual([definition["name"] for definition in by_name["definitions"]], ["value"])

        by_description = event_stream_mcp.inspect(self.path, description_filter="tw")
        self.assertEqual([definition["name"] for definition in by_description["definitions"]], ["two"])
        self.assertEqual(by_description["counts_by_definition"], {"two": 1})

        counted = event_stream_mcp.count(self.path, events=["one"], accumulate=["one/value"])
        self.assertEqual(counted["one"], 2)
        self.assertEqual(counted["one/value"], 12)
        self.assertNotIn("summary", counted)

        interval_counted = event_stream_mcp.count(self.path, events=["one"], interval=20)
        self.assertEqual(interval_counted["one"], 2)
        self.assertEqual(interval_counted["series.10.30.one"], 1)
        self.assertEqual(interval_counted["series.30.50.one"], 1)

    def test_rate_and_latency_return_metric_dictionaries(self):
        latency = event_stream_mcp.latency(self.path, events=["one", "two"], key="txid")
        self.assertEqual(latency["one_two.count"], 1)
        self.assertEqual(latency["one_two.sum_latency"], 10)
        self.assertNotIn("summary", latency)

        rate = event_stream_mcp.rate(self.path, interval="one", events="two")
        self.assertEqual(rate["two.rate.0"], 1)
        self.assertEqual(rate["two.rate.1"], 1)
        self.assertNotIn("summary", rate)

    def test_removed_tool_options_are_not_in_signatures(self):
        latency_params = inspect.signature(event_stream_mcp.latency).parameters
        self.assertNotIn("prefix", latency_params)
        self.assertNotIn("ignore_missing", latency_params)

        rate_params = inspect.signature(event_stream_mcp.rate).parameters
        self.assertNotIn("suffix", rate_params)

        sample_params = inspect.signature(event_stream_mcp.sample).parameters
        self.assertNotIn("expand_enums", sample_params)

    def test_params_sample_and_cache(self):
        params = event_stream_mcp.params(self.path, all=True)
        self.assertEqual(params["parameters"][0]["name"], "width")
        self.assertEqual(params["parameters"][0]["value"], 4)

        params_by_name = event_stream_mcp.params(self.path, name_filter="ID")
        self.assertEqual([param["name"] for param in params_by_name["parameters"]], ["width"])

        params_by_description = event_stream_mcp.params(self.path, description_filter="machine")
        self.assertEqual([param["name"] for param in params_by_description["parameters"]], ["width"])

        sampled = event_stream_mcp.sample(self.path, events="one", limit=1)
        self.assertEqual(len(sampled["events"]), 1)
        self.assertEqual(sampled["events"][0]["name"], "one")
        self.assertTrue(sampled["truncated"])

        filtered = event_stream_mcp.sample(self.path, events="one", data_filter="state=HIT")
        self.assertEqual(len(filtered["events"]), 1)
        self.assertEqual(filtered["events"][0]["time"], 30)
        self.assertFalse(filtered["truncated"])

        stats = event_stream_mcp.cache("stats")
        self.assertEqual(stats["streams"], 1)


if __name__ == "__main__":
    unittest.main()
