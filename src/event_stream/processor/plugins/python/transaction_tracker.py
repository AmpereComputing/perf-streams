# Copyright (c) 2026, Ampere Computing LLC
# SPDX-License-Identifier: BSD-3-Clause

"""Transaction tracking helpers."""

from collections.abc import Callable

import evp
from perf_streams.event_stream import Event

type TxData = bool | int | float | str


def _noop_tx_callback(txid: int) -> None:
    """Default transaction callback."""


class TransactionTracker:
    """
    TransactionTracker watches transactions, recording parent/child
    relationships between them, and allowing the user to record data items on
    individual transactions when they occur and query the transaction tree
    for recorded data.

    A user (typically a subclass) of TransactionTracker can record data
    against a given transaction using add_tx_data(), and query that data using
    get_tx_data(), optionally also searching that transaction's direct
    ancestors with search_parents=True.

    TransactionTracker holds onto any parent transaction and its data as long
    as that transaction has any active children. This allows child transactions
    to query data items on their parents/ancestors even if those ancestors
    transactions have already ended.
    """

    def __init__(
        self,
        on_start_tx: Callable[[int], None] | None = None,
        on_end_tx: Callable[[int], None] | None = None,
    ) -> None:
        """Initialize transaction tracking and hook into start/end events."""
        self.parents: dict[int, int] = {}  # maps child txid to immediate parent txid
        self.children: dict[int, set[int]] = {}  # maps parent txid to immediate children txid's
        self.tx_data: dict[int, dict[str, TxData]] = {}  # maps txid to recorded data for that transaction

        # callback functions for start/end transactions
        self.start_tx_callback = on_start_tx or _noop_tx_callback
        self.end_tx_callback = on_end_tx or _noop_tx_callback

        evp.on("start_transaction", self._start_tx)
        evp.on("end_transaction", self._end_tx)
        evp.end_simulation(self._end_transactions_at_end)

    def _start_tx(self, event: Event) -> None:
        txid = event.data["txid"]
        self.tx_data[txid] = {}
        if "parent" in event.data:
            parent = event.data["parent"]
            self.parents[txid] = parent
            if parent not in self.children:
                self.children[parent] = set()
            self.children[parent].add(txid)

        self.start_tx_callback(txid)

    def add_tx_data(self, txid: int, key: str, value: TxData, ancestor_generations: int = 0) -> None:
        """Add the data item to the tracked transaction, and optionally up to
        `ancestor_generations` levels of parents, if they exist.
        """
        for _generation in range(ancestor_generations + 1):
            if txid in self.tx_data:
                self.tx_data[txid][key] = value

            # Recurse if parent exists, exit otherwise
            if txid in self.parents:
                txid = self.parents[txid]
            else:
                return

    def get_tx_data(self, txid: int, key: str, *, search_parents: bool = False) -> TxData | None:
        """Get data recorded for a transaction, optionally searching parents."""
        if txid not in self.tx_data:
            return None

        tx_data = self.tx_data[txid]
        if key in tx_data:
            return tx_data[key]
        if search_parents and txid in self.parents:
            return self.get_tx_data(self.parents[txid], key, search_parents=True)
        return None

    def _remove_ghost_transactions(self, txid: int) -> None:
        # Do not remove this transaction if it still has children which are
        # still tracked. Effectively, ensure that a transaction's data in
        # self.tx_data outlives all of its children. Then, whenever a
        # transaction ends (and it has no remaining living children),
        # recursively remove any parents which also no longer have children.

        # First, make sure the current transaction has ended (i.e. "__ghost__"
        # is set). If it hasn't ended, we can't remove it or any of its
        # parents.
        if not self.get_tx_data(txid, "__ghost__"):
            return

        # Next, make sure that this transaction does not have any active
        # children. If it does, we can't remove it or any of its parents.
        if txid in self.children:
            return

        # If this transaction has a parent, remove ourselves as one of its
        # active children (removing the set of children if we are the last
        # active child).
        if txid in self.parents:
            parent = self.parents[txid]
            del self.parents[txid]
            self.children[parent].remove(txid)
            if len(self.children[parent]) == 0:
                del self.children[parent]

            # recurse, removing any parents which are ghosts and have no living
            # children
            self._remove_ghost_transactions(parent)

        # finally, remove this transaction's data since we are done with it
        del self.tx_data[txid]

    def _end_tx(self, event: Event) -> None:
        txid = event.data["txid"]
        if txid not in self.tx_data:
            return

        self.end_tx_callback(txid)

        self.add_tx_data(txid, "__ghost__", True)  # noqa: FBT003
        self._remove_ghost_transactions(txid)

    def _end_transactions_at_end(self) -> None:
        for txid in self.tx_data:
            self.end_tx_callback(txid)
