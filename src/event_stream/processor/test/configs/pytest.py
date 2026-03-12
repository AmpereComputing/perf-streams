# Copyright (c) 2026, Ampere Computing LLC
# SPDX-License-Identifier: BSD-3-Clause

import evp


def print_one(event):
    print(f"saw one at {event.time}")


def print_all(event):
    data1 = event.data.get("data1", 0)
    data2 = event.data.get("data2", 0)
    print(f"event {event.name} at {event.time} with data1={data1} and data2={data2}")


evp.on("one", print_one)
evp.on("*", print_all)
