# Copyright (c) 2026, Ampere Computing LLC
# SPDX-License-Identifier: BSD-3-Clause

import evp


def say_hello():
    """hello_to is a global that should be set by evp from the command line"""
    print(f"hello {hello_to}")


evp.end_simulation(say_hello)
