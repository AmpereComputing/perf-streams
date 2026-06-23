// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "protobuf_stream.h"

#include "protobuf_utils/compressed_fstream.h"
#include "protobuf_utils/protobuf_utils.h"

#include <boost/process/v1/search_path.hpp>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fmt/format.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unistd.h>

namespace perf_streams::protobuf_utils {

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

    if (filepath.extension() == ".xz") {
        if (fs::exists(filepath)) {
            if (!force) {
                std::cerr << "ERROR: File already exists: " << filepath.string() << "\n";
                throw std::system_error(errno, std::system_category(), filepath.string());
            }

            std::remove(filepath.c_str());
        }

        fp = open_xz_output_file(filepath.c_str());
    } else {
        if (fs::exists(filepath)) {
            if (!force) {
                std::cerr << "ERROR: File already exists: " << filepath.string() << "\n";
                throw std::system_error(errno, std::system_category(), filepath.string());
            }

            std::remove(filepath.c_str());
        }

        fp = fopen(filepath.c_str(), "wx");
        if (!fp) {
            throw std::system_error(errno, std::system_category(), filepath.string());
        }
    }
    output_stream = std::make_unique<google::protobuf::io::FileOutputStream>(fileno(fp));

    write_header_to(magic_number, version, output_stream);
    flush();
}

ProtobufStreamWriter::~ProtobufStreamWriter()
{
    flush();
    if (fp != nullptr) {
        if (pclose(fp) == -1) {
            perror("Error calling pclose on xz's fd");
        }
    }
    output_stream->Close();
}

bool ProtobufStreamWriter::flush()
{
    return output_stream->Flush();
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

    if (filepath.extension() == ".xz") {
        if (!fs::exists(filepath)) {
            throw std::system_error(errno, std::system_category(), filepath.string());
        }

        fp = open_xz_input_file(filepath.c_str());
    } else if (filepath.extension() == ".gz") {
        if (!fs::exists(filepath)) {
            throw std::system_error(errno, std::system_category(), filepath.string());
        }

        auto gunzippath = boost::process::v1::search_path("gunzip");
        if (gunzippath.empty()) {
            throw std::system_error(errno, std::system_category(), "Cannot find `gunzip` on $PATH");
        }

        fp = popen(fmt::format("{} -c {}", gunzippath.string(), filepath.string()).c_str(), "r");
        if (fp == nullptr) {
            throw std::system_error(errno, std::system_category(), "popen() failed");
        }
    } else {
        fp = fopen(filepath.c_str(), "r");
        if (!fp) {
            throw std::system_error(errno, std::system_category(), filepath.string());
        }
    }
    input_stream = std::make_unique<google::protobuf::io::FileInputStream>(fileno(fp));
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
    int status(0);
    char buf[256];
    if (fp != nullptr) {
        status = pclose(fp);
        if (status == -1) {
            snprintf(buf, sizeof(buf), "Error calling pclose on xzcat or gunzip's fd (%s)", strerror(status));
            perror(buf);
        }
    } else if (fd != -1) {
        status = close(fd);
        if (status == -1) {
            perror("Error closing event stream file descriptor");
        }
    }
    input_stream->Close();
}

void ProtobufStreamReader::try_close()
{
    if (fp != nullptr) {
        int const status = pclose(fp);
        if (status != 0) {
            std::string msg("error reading from xzcat or gunzip for ");
            msg += filepath.string();
            throw std::system_error(errno, std::system_category(), msg);
        }
        fp = nullptr;
    }
}

} // namespace perf_streams::protobuf_utils
