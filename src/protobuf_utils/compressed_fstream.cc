// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "compressed_fstream.h"

#include <algorithm>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/lzma.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>

namespace perf_streams::protobuf_utils {

namespace {

unsigned compression_threads(unsigned max)
{
    return std::min(max, std::max(1U, std::thread::hardware_concurrency()));
}

auto lzma_params(unsigned max_threads = 4)
{
    boost::iostreams::lzma_params params{};
    params.threads = compression_threads(max_threads);
    return params;
}

void push_ostream_compressor(boost::iostreams::filtering_ostream& out, CompressionType compression)
{
    if (compression == CompressionType::GZIP)
        out.push(boost::iostreams::gzip_compressor());
    else if (compression == CompressionType::ZLIB)
        out.push(boost::iostreams::zlib_compressor());
    else if (compression == CompressionType::BZ2)
        out.push(boost::iostreams::bzip2_compressor());
    else if (compression == CompressionType::XZ)
        out.push(boost::iostreams::lzma_compressor(lzma_params()));
}

} // namespace

CompressionType compression_from_filename(const char* filename)
{
    std::filesystem::path const filepath{filename};
    auto extension = filepath.extension();
    if (extension == ".gz") {
        return CompressionType::GZIP;
    } else if (extension == ".zlib" || extension == ".zl") {
        return CompressionType::ZLIB;
    } else if (extension == ".bz2") {
        return CompressionType::BZ2;
    } else if (extension == ".xz") {
        return CompressionType::XZ;
    }
    return CompressionType::NONE;
}

std::unique_ptr<std::ostream> open_compressed_ostream(const char* filename,
                                                      std::ios_base::openmode mode,
                                                      std::optional<CompressionType> compression)
{
    auto compression_type = compression.value_or(compression_from_filename(filename));

    if (std::strlen(filename) == 1 && filename[0] == '-') {
        auto out = std::make_unique<boost::iostreams::filtering_ostream>();
        push_ostream_compressor(*out, compression_type);
        out->push(boost::ref(std::cout));
        return out;
    }

    if (compression_type == CompressionType::NONE)
        return std::make_unique<std::ofstream>(filename, mode);

    if ((mode & std::ios_base::app) || !(mode & std::ios_base::trunc))
        throw std::runtime_error("Compressed output streams require truncation; must not append.");

    auto out = std::make_unique<boost::iostreams::filtering_ostream>();
    push_ostream_compressor(*out, compression_type);
    out->push(boost::iostreams::file_sink(filename, mode | std::ios_base::binary));

    return out;
}

std::unique_ptr<std::istream> open_compressed_istream(const char* filename, std::ios_base::openmode mode)
{
    auto compression = compression_from_filename(filename);

    if (compression == CompressionType::NONE)
        return std::make_unique<std::ifstream>(filename, mode);

    auto in = std::make_unique<boost::iostreams::filtering_istream>();
    if (compression == CompressionType::GZIP)
        in->push(boost::iostreams::gzip_decompressor());
    else if (compression == CompressionType::ZLIB)
        in->push(boost::iostreams::zlib_decompressor());
    else if (compression == CompressionType::BZ2)
        in->push(boost::iostreams::bzip2_decompressor());
    else if (compression == CompressionType::XZ)
        in->push(boost::iostreams::lzma_decompressor(lzma_params()));

    in->push(boost::iostreams::file_source(filename, mode | std::ios_base::binary));

    return in;
}

} // namespace perf_streams::protobuf_utils
