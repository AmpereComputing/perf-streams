/*
 * Copyright (c) 2026, Ampere Computing LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdio>
#include <ios>
#include <istream>
#include <memory>
#include <ostream>

namespace perf_streams::protobuf_utils {

enum class CompressionType {
    NONE = 0,
    GZIP,
    ZLIB,
    BZ2,
    XZ,
};

CompressionType compression_from_filename(const char* filename);
std::unique_ptr<std::ostream> open_compressed_ostream(const char* filename,
                                                      std::ios_base::openmode mode = std::ios_base::out
                                                                                     | std::ios_base::trunc);
std::unique_ptr<std::istream> open_compressed_istream(const char* filename,
                                                      std::ios_base::openmode mode = std::ios_base::in);

FILE* open_xz_output_file(const char* filename);
FILE* open_xz_input_file(const char* filename);

} // namespace perf_streams::protobuf_utils
