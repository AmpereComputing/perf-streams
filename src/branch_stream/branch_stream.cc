// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "branch_stream.h"

#include "instruction_stream/instruction_stream.h"
#include "protobuf_utils/protobuf_stream.h"

#include <cstdint>
#include <filesystem>

namespace perf_streams::branch_stream {

const uint32_t protobuf_magic = 0x53424250; // 0x50(P) 0x42(B) 0x42(B) 0x53(S)
const uint32_t protobuf_bs_version = 1;

BranchStreamWriter::BranchStreamWriter(std::filesystem::path filepath, bool force)
    : ProtobufStreamWriter(filepath, protobuf_magic, protobuf_bs_version, force)
{
}

bool BranchStreamWriter::write(const Branch& branch)
{
    Event event;
    event.mutable_branch()->CopyFrom(branch);
    return write(event);
}

bool BranchStreamWriter::write(const instruction_stream::Context& context)
{
    Event event;
    event.mutable_context()->CopyFrom(context);
    return write(event);
}

bool BranchStreamWriter::write(const instruction_stream::SysReg& sysreg)
{
    Event event;
    event.mutable_sysreg()->CopyFrom(sysreg);
    return write(event);
}

BranchStreamReader::BranchStreamReader(std::filesystem::path filepath)
    : ProtobufStreamReader(filepath, protobuf_magic, protobuf_bs_version)
{
}

bool BranchStreamReader::read(Branch& branch)
{
    Event event;
    bool status = true;
    while (status && !event.has_branch())
        status = read(event);
    if (status) {
        branch.Swap(event.mutable_branch());
    }
    return status;
}

} // namespace perf_streams::branch_stream
