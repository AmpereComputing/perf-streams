/*
 * Copyright (c) 2026, Ampere Computing LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "branch_stream/branch_stream.pb.h" // IWYU pragma: export
#include "instruction_stream/instruction_stream.h"
#include "protobuf_utils/protobuf_stream.h"

#include <filesystem>

namespace perf_streams::branch_stream {

class BranchStreamWriter : public protobuf_utils::ProtobufStreamWriter
{
public:
    explicit BranchStreamWriter(std::filesystem::path filepath, bool force = false);
    bool write(const Branch& branch);
    bool write(const instruction_stream::Context& context);
    bool write(const instruction_stream::SysReg& sysreg);
    bool write(const Event& event) { return ProtobufStreamWriter::write(event); }
};

class BranchStreamReader : public protobuf_utils::ProtobufStreamReader
{
public:
    explicit BranchStreamReader(std::filesystem::path filepath);
    bool read(Branch& branch);
    bool read(Event& event) { return ProtobufStreamReader::read(event); }
};

} // namespace perf_streams::branch_stream
