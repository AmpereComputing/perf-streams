# Copyright (c) 2026, Ampere Computing LLC
# SPDX-License-Identifier: BSD-3-Clause

import evp


evp.require_transactions()


def probe(event):
    txid = evp.event_txid(event)
    parent = evp.transaction_parent(txid)
    print(
        f"{event.name} txid={txid} parent={parent} "
        f"root={evp.is_ancestor(1, txid)} "
        f"related_2_3={evp.is_related(2, 3)} "
        f"related_1={evp.is_related(1, txid)} "
        f"unrelated_99={evp.is_related(txid, 99)}"
    )


def no_tx(event):
    print(f"{event.name} txid={evp.event_txid(event)}")


evp.on("probe", probe)
evp.on("no_tx", no_tx)
