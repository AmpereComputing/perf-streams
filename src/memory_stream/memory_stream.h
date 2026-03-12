/*
 * Copyright (c) 2026, Ampere Computing LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "memory_stream/memory_stream.pb.h" // IWYU pragma: export
#include "protobuf_utils/protobuf_stream.h"

#include <cstdint>
#include <filesystem>

namespace perf_streams::memory_stream {

class MemoryStreamWriter : public protobuf_utils::ProtobufStreamWriter
{
public:
    MemoryStreamWriter(std::filesystem::path filepath,
                       uint32_t dcache_size,
                       uint32_t icache_size,
                       bool unified = false);
    bool write(const Access& access);
    bool write(const Control& control);
    bool write(const Event& event) { return ProtobufStreamWriter::write(event); }
};

class MemoryStreamReader : public protobuf_utils::ProtobufStreamReader
{
public:
    explicit MemoryStreamReader(std::filesystem::path filepath);
    bool read(Access& access);
    bool read(Event& event) { return ProtobufStreamReader::read(event); }
    bool filter_was_unified() const { return unified_filter; }
    uint32_t filter_dcache_size() const { return ms_dcache_size; } // in bytes
    uint32_t filter_icache_size() const { return ms_icache_size; } // in bytes
private:
    // unified_filter is true if the filter cache was unified (in which was the
    // values reported by filter_dcache_size() and filter_icache_size() will be
    // equal and both correspond to the *total* size of the cache.
    bool unified_filter;
    uint32_t ms_dcache_size; // The size, in bytes, of the dcache this memory stream was filtered through
    uint32_t ms_icache_size; // The size, in bytes, of the icache this memory stream was filtered through
};

} // namespace perf_streams::memory_stream
