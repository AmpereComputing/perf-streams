# Copyright (c) 2026, Ampere Computing LLC
# SPDX-License-Identifier: BSD-3-Clause

import evp


def probe(_event):
    evp.require_transactions()


evp.on("probe", probe)
