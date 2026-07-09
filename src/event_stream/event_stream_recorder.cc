// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "event_stream_recorder.h"

#include "event_stream/event_stream.h"

#include <cstdint>
#include <string>

namespace perf_streams::event_stream {

void EventStreamRecorder::record_event(const RecorderEventHandle& event)
{
    RecordedEvent const e(event, this);
    if (callback_func)
        callback_func(e);
    else
        events.emplace_back(e);
}

void EventStreamRecorder::post_event(EventType event_type, std::uint64_t time)
{
    if (listening(event_type))
        record_event({event_type, time});
}

EventHandle EventStreamRecorder::open_event(EventType event_type, std::uint64_t time)
{
    if (listening(event_type))
        return event_handle_pool.construct(event_type, time);
    return event_handle_pool.construct();
}

void EventStreamRecorder::close_event(EventHandle event)
{
    auto* handle = static_cast<RecorderEventHandle*>(event);
    if (*handle)
        record_event(*handle);
    event_handle_pool.destroy(handle);
}

void EventStreamRecorder::add_int_data(EventHandle event, DataType event_data_type, std::int64_t value)
{
    auto* handle = static_cast<RecorderEventHandle*>(event);
    if (*handle)
        handle->data[event_data_type] = value;
}

void EventStreamRecorder::add_uint_data(EventHandle event, DataType event_data_type, std::uint64_t value)
{
    auto* handle = static_cast<RecorderEventHandle*>(event);
    if (*handle)
        handle->data[event_data_type] = value;
}

void EventStreamRecorder::add_string_data(EventHandle event, DataType event_data_type, const std::string& value)
{
    auto* handle = static_cast<RecorderEventHandle*>(event);
    if (*handle)
        handle->data[event_data_type] = value;
}
} // namespace perf_streams::event_stream
