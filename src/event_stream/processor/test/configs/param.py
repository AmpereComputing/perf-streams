# Copyright (c) 2026, Ampere Computing LLC
# SPDX-License-Identifier: BSD-3-Clause

import evp


def print_param(name):
    value = evp.get_parameter(name)
    print(type(value).__name__, value)


print_param("core.machine_width")
print_param("core.rob.size")
print_param("core.scheduler_configuration")
print_param("missing")
