# Copyright (c) 2026, Ampere Computing LLC
# SPDX-License-Identifier: BSD-3-Clause

import evp

evp.require_transactions()


def probe(event):
    print(
        f"{event.name} parent_3={evp.transaction_parent(3)} "
        f"root_3={evp.is_ancestor(1, 3)} related_1_3={evp.is_related(1, 3)}"
    )


evp.on("probe_after_middle_end", probe)
