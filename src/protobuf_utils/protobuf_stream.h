/*
 * Copyright (c) 2026, Ampere Computing LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "protobuf_utils.h"

#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <istream>
#include <memory>
#include <ostream>
#include <string>
#include <system_error>

namespace perf_streams::protobuf_utils {

class ProtobufStreamWriter
{
public:
    ProtobufStreamWriter(int fd, uint32_t magic_number, uint32_t version);
    ProtobufStreamWriter(std::filesystem::path filepath, uint32_t magic_number, uint32_t version, bool force = false);
    ~ProtobufStreamWriter();
    template<typename Item>
    bool write(const Item& item);
    bool flush();

protected:
    std::filesystem::path filepath;
    std::unique_ptr<std::ostream> owned_output_stream;
    std::unique_ptr<google::protobuf::io::CopyingOutputStream> copying_output_stream;
    std::unique_ptr<google::protobuf::io::CopyingOutputStreamAdaptor> output_stream;
};

template<typename Item>
bool ProtobufStreamWriter::write(const Item& item)
{
    return write_delimited_to(item, output_stream);
}

class ProtobufStreamReader
{
public:
    ProtobufStreamReader(int fd, uint32_t magic_number, uint32_t max_version);
    ProtobufStreamReader(std::filesystem::path filepath, uint32_t magic_number, uint32_t max_version);
    ~ProtobufStreamReader();
    uint32_t version() const { return stream_version; }
    template<typename Item>
    bool read(Item& item);

protected:
    std::filesystem::path filepath;
    std::unique_ptr<std::istream> owned_input_stream;
    std::unique_ptr<google::protobuf::io::ZeroCopyInputStream> input_stream;
    std::unique_ptr<DelimitedReader> reader;

private:
    void verify_file_version(uint32_t magic_number, uint32_t max_version);
    void try_close();
    uint32_t stream_version;
};

template<typename Item>
bool ProtobufStreamReader::read(Item& item)
{
    bool const status = reader->read(&item);
    if (!status)
        try_close();
    return status;
}

} // namespace perf_streams::protobuf_utils
