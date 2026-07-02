# Copyright (c) 2026, Ampere Computing LLC
# SPDX-License-Identifier: BSD-3-Clause

import evp


def probe(event):
    evp.event_txid(event)


evp.on("probe", probe)
