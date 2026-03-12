# Copyright (c) 2026, Ampere Computing LLC
# SPDX-License-Identifier: BSD-3-Clause

import os
import unittest
from enum import Enum
from tempfile import TemporaryDirectory

import perf_streams.event_stream_pb2 as es_proto
from perf_streams.event_stream import EventStreamReader, EventStreamWriter, Transaction

testdir = os.path.dirname(__file__)


class TestEventStreamReader(unittest.TestCase):
    def setUp(self):
        self.event = es_proto.Event()
        self.reader = EventStreamReader(os.path.join(testdir, "streams", "four_events_with_data.es"))

    def test_read_one_event(self):
        self.reader.read_event(self.event)
        self.assertEqual(self.event.definition_id, 1)
        self.assertEqual(self.event.time, 200)
        self.assertEqual(len(self.event.values), 2)

    def test_read_events(self):
        events = list(self.reader.read_events({"foo"}))
        self.assertEqual(len(events), 2)
        self.assertEqual(events[0].name, "foo")
        self.assertEqual(
            events[0].data,
            {
                "bar": 1234,
                "baz": 5678,
            },
        )
        self.assertEqual(events[1].time, 400)
        self.assertEqual(events[1].data, {})


class Phase(Enum):
    FETCH = 0
    EXEC = 1


class CacheState(Enum):
    MISS = 2
    HIT = 3


class TestEventStreamWriter(unittest.TestCase):
    def setUp(self):
        self.work = TemporaryDirectory()
        self.output_file = os.path.join(self.work.name, "python_event_stream_writer.es")

    def test_write_enumerated_event(self):
        writer = EventStreamWriter(self.output_file)
        event_type = writer.define_event("foo", "Foo event")
        phase_enumeration = writer.define_enumeration(Phase)
        cache_enumeration = writer.define_enumeration(CacheState)
        self.assertEqual(phase_enumeration[Phase.FETCH.value], Phase.FETCH.name)
        self.assertIsInstance(phase_enumeration.enumeration_id, int)
        phase_value = writer.define_data("phase", "Pipeline phase", enumeration=phase_enumeration)
        cache_value = writer.define_data("cache_state", "Cache state", enumeration=cache_enumeration)
        writer.set_parameter("model_name", "catscan")
        writer.start_simulation()

        writer.post_event(
            event_type,
            time=100,
            values={phase_value: Phase.FETCH, cache_value: CacheState.MISS},
        )
        writer.post_event(
            event_type,
            time=200,
            values={phase_value: Phase.EXEC, cache_value: CacheState.HIT},
        )
        writer.close()

        reader = EventStreamReader(self.output_file, convert_enumerations=True)
        events = list(reader.read_events({"foo"}))
        self.assertEqual(len(events), 2)
        self.assertEqual(events[0].data["phase"], Phase.FETCH.name)
        self.assertEqual(events[0].data["cache_state"], CacheState.MISS.name)
        self.assertEqual(events[1].data["phase"], Phase.EXEC.name)
        self.assertEqual(events[1].data["cache_state"], CacheState.HIT.name)
        self.assertEqual(reader.find_parameter_by_name("model_name").string_value, "catscan")

    def test_post_event_requires_start_simulation(self):
        writer = EventStreamWriter(self.output_file)
        event_type = writer.define_event("foo", "Foo event")

        with self.assertRaisesRegex(
            RuntimeError,
            r"post_event is only allowed after START_SIMULATION",
        ):
            writer.post_event(event_type, time=1)

        writer.start_simulation()

        writer.post_event(event_type, time=1)

        writer.close()

    def test_post_event_requires_dict_values(self):
        writer = EventStreamWriter(self.output_file)
        event_type = writer.define_event("foo", "Foo event")
        value_type = writer.define_data("field", "Foo field")
        writer.start_simulation()

        with self.assertRaisesRegex(
            TypeError,
            r"values must be dict.*, but was list",
        ):
            writer.post_event(event_type, time=1, values=[(value_type.name, 1)])

        with self.assertRaisesRegex(
            TypeError,
            r"values must be dict.*, but was list",
        ):
            writer.post_event(event_type, time=1, values=[value_type.name, 1])

        writer.post_event(event_type, time=1, values={value_type: 1})

        writer.close()

    def test_post_event_requires_event_type_handle(self):
        writer = EventStreamWriter(self.output_file)
        event_type = writer.define_event("foo", "Foo event")
        writer.start_simulation()

        with self.assertRaisesRegex(
            TypeError,
            r"event_type must be EventType, but was str",
        ):
            writer.post_event("foo", time=1)

        with self.assertRaisesRegex(
            TypeError,
            r"event_type must be EventType, but was int",
        ):
            writer.post_event(event_type.id, time=1)

        writer.post_event(event_type, time=1)

        writer.close()

    def test_post_event_requires_value_type_handles(self):
        writer = EventStreamWriter(self.output_file)
        event_type = writer.define_event("foo", "Foo event")
        value_type = writer.define_data("phase", "Pipeline phase")
        writer.start_simulation()

        with self.assertRaisesRegex(
            TypeError,
            r"value_type must be ValueType, but was int",
        ):
            writer.post_event(event_type, time=1, values={value_type.id: 1})

        with self.assertRaisesRegex(
            TypeError,
            r"value_type must be ValueType, but was str",
        ):
            writer.post_event(event_type, time=2, values={value_type.name: 2})

        writer.post_event(event_type, time=3, values={value_type: 3})

        writer.close()

    def test_define_data_requires_enumeration_handles(self):
        writer = EventStreamWriter(self.output_file)

        with self.assertRaisesRegex(
            TypeError,
            r"enumeration must be Enumeration, but was \w+",
        ):
            writer.define_data("phase", "Pipeline phase", enumeration=Phase)

        with self.assertRaisesRegex(
            TypeError,
            r"enumeration must be Enumeration, but was dict",
        ):
            writer.define_data("phase", "Pipeline phase", enumeration={0: "FETCH", 1: "EXEC"})

        writer.define_enumeration(Phase)

        writer.close()

    def test_define_enumeration_value_matches_enumeration_id(self):
        writer = EventStreamWriter(self.output_file)

        phase_enumeration = writer.define_enumeration(Phase)
        phase_value = writer.define_data("phase", "Pipeline phase", enumeration=phase_enumeration)
        self.assertEqual(phase_value.enumeration_id, phase_enumeration.enumeration_id)

        writer.close()

    def test_transaction_methods_require_dict_values(self):
        writer = EventStreamWriter(self.output_file)
        value_type = writer.define_data("key", "Some key")
        writer.start_simulation()

        with self.assertRaisesRegex(
            TypeError,
            r"values must be dict.*, but was list",
        ):
            writer.begin_transaction(time=10, values=[("key", "value")])

        transaction = writer.begin_transaction(time=20, values={value_type: "value"})

        with self.assertRaisesRegex(
            TypeError,
            r"values must be dict.*, but was list",
        ):
            writer.end_transaction(transaction, time=30, values=[("key", "value")])

        writer.end_transaction(transaction, time=30, values={value_type: "value"})

        writer.close()

    def test_write_parent_child_transactions(self):
        writer = EventStreamWriter(self.output_file)
        work_event = writer.define_event("work", "work event")
        stage_value = writer.define_data("stage", "work stage")
        writer.start_simulation()

        parent_txn = writer.begin_transaction(time=10)
        child_txn = writer.begin_transaction(time=20, parent=parent_txn)
        writer.post_event(work_event, time=30, values={stage_value: "decode"}, transaction=child_txn)
        writer.end_transaction(child_txn, time=40)
        writer.end_transaction(parent_txn, time=50)
        writer.close()

        reader = EventStreamReader(self.output_file)
        transactions = list(
            reader.read_transactions({Transaction.DEFAULT_START_NAME, Transaction.DEFAULT_END_NAME, "work"})
        )
        transactions_by_id = {transaction.txid: transaction for transaction in transactions}

        self.assertEqual(set(transactions_by_id.keys()), {0, 1})
        self.assertEqual(transactions_by_id[1].parent.txid, 0)
        self.assertEqual(
            {event.name for event in transactions_by_id[1].events}, {"start_transaction", "work", "end_transaction"}
        )
