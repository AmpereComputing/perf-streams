/*
 * Copyright (c) 2026, Ampere Computing LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

namespace perf_streams::event_stream {

class Event
{
public:
    virtual ~Event() = default;
};

using EventHandle = Event*;

} // namespace perf_streams::event_stream
