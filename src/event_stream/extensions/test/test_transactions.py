# Copyright (c) 2026, Ampere Computing LLC
# SPDX-License-Identifier: BSD-3-Clause

import os
import unittest
from tempfile import TemporaryDirectory

from perf_streams.event_stream import (
    EventStreamReader,
    EventStreamWriter,
    EventStreamWriterWith,
    Transaction,
    event_stream_writer_extensions_valid,
)
from perf_streams.extensions.transactions import Transactions


class TestTransactionWriterExtension(unittest.TestCase):
    def setUp(self):
        self.work = TemporaryDirectory()
        self.output_file = os.path.join(self.work.name, "python_event_stream_writer.es")

    def test_default_writer_supports_transactions(self):
        writer = EventStreamWriter(self.output_file)

        self.assertTrue(hasattr(writer, "begin_transaction"))
        self.assertTrue(hasattr(writer, "end_transaction"))

        writer.close()

    def test_bare_writer_excludes_transactions(self):
        bare_writer = EventStreamWriterWith()
        writer = bare_writer(self.output_file)
        event_type = writer.define_event("work", "work event")
        writer.start_simulation()

        self.assertFalse(hasattr(writer, "begin_transaction"))
        self.assertFalse(hasattr(writer, "end_transaction"))
        writer.post_event(event_type, time=10)

        with self.assertRaisesRegex(TypeError, "transaction"):
            writer.post_event(event_type, time=20, transaction=Transaction(0))

        writer.close()

        reader = EventStreamReader(self.output_file)
        events = list(reader.read_events({"work"}))
        self.assertEqual(len(events), 1)

    def test_transaction_extension_writer_writes_transactions(self):
        transaction_writer = EventStreamWriterWith(Transactions)
        writer = transaction_writer(self.output_file)
        work_event = writer.define_event("work", "work event")
        writer.start_simulation()

        transaction = writer.begin_transaction(time=10)
        writer.post_event(work_event, time=20, transaction=transaction)
        writer.end_transaction(transaction, time=30)
        writer.close()

        reader = EventStreamReader(self.output_file)
        transactions = list(
            reader.read_transactions({Transaction.DEFAULT_START_NAME, Transaction.DEFAULT_END_NAME, "work"})
        )

        self.assertEqual(len(transactions), 1)
        self.assertEqual(
            {event.name for event in transactions[0].events}, {"start_transaction", "work", "end_transaction"}
        )

    def test_writer_extension_requirements(self):
        class BaseExtension:
            def extension_order(self):
                return ["base"]

        class DependentExtension:
            requirements = (BaseExtension,)

            def extension_order(self):
                return [*super().extension_order(), "dependent"]

        writer_type = EventStreamWriterWith(BaseExtension, DependentExtension)
        writer = writer_type(self.output_file)

        self.assertTrue(event_stream_writer_extensions_valid(BaseExtension, DependentExtension))
        self.assertEqual(writer.extension_order(), ["base", "dependent"])
        writer.close()

        self.assertFalse(event_stream_writer_extensions_valid(DependentExtension))
        self.assertFalse(event_stream_writer_extensions_valid(DependentExtension, BaseExtension))

        with self.assertRaises(ValueError):
            EventStreamWriterWith(DependentExtension)

        with self.assertRaises(ValueError):
            EventStreamWriterWith(DependentExtension, BaseExtension)
