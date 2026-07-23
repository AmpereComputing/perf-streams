// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "protobuf_stream.h"

#include "protobuf_utils/compressed_fstream.h"
#include "protobuf_utils/protobuf_utils.h"

#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>

namespace perf_streams::protobuf_utils {

namespace {

class OstreamCopyingOutputStream final : public google::protobuf::io::CopyingOutputStream
{
public:
    explicit OstreamCopyingOutputStream(std::ostream* output) : output(output) {}

    bool Write(const void* buffer, int size) override
    {
        output->write(static_cast<const char*>(buffer), size);
        return output->good();
    }

private:
    std::ostream* output;
};

} // namespace

ProtobufStreamWriter::ProtobufStreamWriter(int fd, uint32_t magic_number, uint32_t version)
    : output_stream{std::make_unique<google::protobuf::io::FileOutputStream>(fd)}
{
    write_header_to(magic_number, version, output_stream);
    flush();
}

ProtobufStreamWriter::ProtobufStreamWriter(std::filesystem::path filepath,
                                           uint32_t magic_number,
                                           uint32_t version,
                                           bool force)
    : filepath(filepath)
{
    namespace fs = std::filesystem;

    if (fs::exists(filepath)) {
        if (!force) {
            std::cerr << "ERROR: File already exists: " << filepath.string() << "\n";
            throw std::system_error(errno, std::system_category(), filepath.string());
        }

        fs::remove(filepath);
    }

    owned_output_stream = open_compressed_ostream(filepath.c_str(), std::ios_base::out | std::ios_base::trunc);
    if (!owned_output_stream || !*owned_output_stream)
        throw std::system_error(errno, std::system_category(), filepath.string());
    copying_output_stream = std::make_unique<OstreamCopyingOutputStream>(owned_output_stream.get());
    output_stream = std::make_unique<google::protobuf::io::CopyingOutputStreamAdaptor>(copying_output_stream.get());

    write_header_to(magic_number, version, output_stream);
    flush();
}

ProtobufStreamWriter::~ProtobufStreamWriter()
{
    flush();
    output_stream.reset();
    copying_output_stream.reset();
    if (owned_output_stream) {
        owned_output_stream->flush();
        owned_output_stream.reset();
    }
}

bool ProtobufStreamWriter::flush()
{
    if (!output_stream)
        return true;

    bool ok = output_stream->Flush();
    if (owned_output_stream) {
        owned_output_stream->flush();
        ok = ok && owned_output_stream->good();
    }
    return ok;
}

ProtobufStreamReader::ProtobufStreamReader(int fd, uint32_t magic_number, uint32_t max_version)
    : input_stream{std::make_unique<google::protobuf::io::FileInputStream>(fd)},
      reader{std::make_unique<DelimitedReader>(input_stream)}
{
    verify_file_version(magic_number, max_version);
}

ProtobufStreamReader::ProtobufStreamReader(std::filesystem::path filepath, uint32_t magic_number, uint32_t max_version)
    : filepath(filepath)
{
    namespace fs = std::filesystem;

    if (!fs::exists(filepath))
        throw std::system_error(errno, std::system_category(), filepath.string());

    owned_input_stream = open_compressed_istream(filepath.c_str(), std::ios_base::in);
    if (!owned_input_stream || !*owned_input_stream)
        throw std::system_error(errno, std::system_category(), filepath.string());
    input_stream = std::make_unique<google::protobuf::io::IstreamInputStream>(owned_input_stream.get());
    reader = std::make_unique<DelimitedReader>(input_stream);

    verify_file_version(magic_number, max_version);
}

void ProtobufStreamReader::verify_file_version(uint32_t magic_number, uint32_t max_version)
{
    uint32_t magic;
    reader->read_header(&magic, &stream_version);
    if (magic != magic_number) {
        throw std::runtime_error("Unrecognized magic number for protobuf stream");
    }
    if (stream_version > max_version) {
        throw std::runtime_error("Unrecognized protobuf stream version: " + std::to_string(stream_version));
    }
}

ProtobufStreamReader::~ProtobufStreamReader()
{
    reader.reset();
    input_stream.reset();
    owned_input_stream.reset();
}

void ProtobufStreamReader::try_close()
{
    if (owned_input_stream && owned_input_stream->bad())
        throw std::runtime_error("error reading from compressed stream for " + filepath.string());
}

} // namespace perf_streams::protobuf_utils
