/*
 * Copyright (c) 2026, Ampere Computing LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "instruction_stream/instruction_stream.pb.h" // IWYU pragma: export
#include "protobuf_utils/protobuf_stream.h"

#include <filesystem>

namespace perf_streams::instruction_stream {

class InstructionStreamWriter : public protobuf_utils::ProtobufStreamWriter
{
public:
    InstructionStreamWriter(std::filesystem::path filepath, const Features& features, bool force = false);
    bool write(const Instruction& instruction);
    bool write(const Memory& memory);
    bool write(const Control& control);
    bool write(const SysReg& sysreg);
    bool write(const Context& context);
    bool write(const Event& event);
    void finalize();
    ~InstructionStreamWriter();

private:
    bool started_measurement, stopped_measurement, finalize_called;
};

class InstructionStreamReader : public protobuf_utils::ProtobufStreamReader
{
public:
    InstructionStreamReader(std::filesystem::path filepath);
    Features features() { return stream_features; }
    bool read(Instruction& instruction);
    bool read(Event& event) { return ProtobufStreamReader::read(event); }

private:
    Features stream_features;
};

} // namespace perf_streams::instruction_stream
