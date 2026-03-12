// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "disasm.h"

#include <capstone.h>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <fmt/base.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <string>

namespace perf_streams::disasm {

static csh disasm_handle;

std::string disasm(std::uint32_t bytes, std::uint64_t pc)
{
    size_t count(0);
    cs_insn* insn;
    std::string inst_dis;

    // place the instruction bytes into the format required by capstone
    uint8_t byte_array[4];
    byte_array[0] = bytes;
    byte_array[1] = bytes >> 8;
    byte_array[2] = bytes >> 16;
    byte_array[3] = bytes >> 24;

    count = cs_disasm(disasm_handle, byte_array, 4, pc, 0, &insn);

    if (count) {
        // format the disasm and write it to the record
        inst_dis = fmt::format("{:<5} {}", insn[0].mnemonic, insn[0].op_str);

        // free memory allocated by cs_disasm()
        cs_free(insn, count);

    } else {
        inst_dis = fmt::format("[undefined opcode: {:08x}]", bytes);
    }

    return inst_dis;
}

void init()
{
    // set up capstone for disassembly
    cs_err err = cs_open(CS_ARCH_ARM64, CS_MODE_ARM, &disasm_handle);
    if (err) {
        fmt::print("Failed on cs_open() with error returned: {}\n", static_cast<unsigned>(err));
        assert(0);
    }
    cs_option(disasm_handle, CS_OPT_DETAIL, CS_OPT_ON);
}

} // namespace perf_streams::disasm
