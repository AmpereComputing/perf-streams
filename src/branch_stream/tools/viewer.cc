// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "branch_stream/branch_stream.h"
#include "instruction_stream/instruction_stream.h"

#include <cstdlib>
#include <fcntl.h>
#include <fmt/base.h>
#include <fmt/format.h>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// FIXME: remove using namespace?
using namespace perf_streams;
using namespace perf_streams::branch_stream;

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cout << "branch_stream_viewer [branch stream]" << '\n';
        exit(1);
    }

    BranchStreamReader reader(argv[1]);
    Event event;

    long branch_count = 0, taken_count = 0, sysreg_count = 0, context_count = 0;
    long type_counts[7] = {0};
    while (reader.read(event)) {
        if (event.has_branch()) {
            const auto& branch = event.branch();
            branch_count++;
            type_counts[branch.type()]++;
            if (branch.taken())
                taken_count++;

            fmt::print("Branch: {:7} type: {:22} program counter: 0x{:016x} target: 0x{:x}\n",
                       branch.taken() ? "taken" : "untaken",
                       Branch::Type_Name(branch.type()),
                       branch.program_counter(),
                       branch.target());
        } else if (event.has_sysreg()) {
            const auto& sysreg = event.sysreg();
            sysreg_count++;
            fmt::print("SysReg: op0: {} op1: {} crn: {} crm: {}, op2: {}, value: 0x{:x}\n",
                       sysreg.op0(),
                       sysreg.op1(),
                       sysreg.crn(),
                       sysreg.crm(),
                       sysreg.op2(),
                       sysreg.value());
        } else if (event.has_context()) {
            const auto& context = event.context();
            context_count++;
            fmt::print("Context: type: {} value: {}\n",
                       instruction_stream::Context::Type_Name(context.type()),
                       context.value());
        }

        event.Clear();
    }

    std::cout << std::dec << '\n';
    std::cout << "total branches = " << branch_count << '\n';
    std::cout << "taken branches = " << taken_count << '\n';
    std::cout << "conditional direct branches = " << type_counts[Branch_Type_CONDITIONAL_DIRECT] << '\n';
    std::cout << "unconditional direct branches = " << type_counts[Branch_Type_UNCONDITIONAL_DIRECT] << '\n';
    std::cout << "unconditional indirect branches = " << type_counts[Branch_Type_UNCONDITIONAL_INDIRECT] << '\n';
    std::cout << "call direct branches = " << type_counts[Branch_Type_CALL_DIRECT] << '\n';
    std::cout << "call indirect branches = " << type_counts[Branch_Type_CALL_INDIRECT] << '\n';
    std::cout << "return branches = " << type_counts[Branch_Type_RETURN] << '\n';
    std::cout << "total sysregs = " << sysreg_count << '\n';
    std::cout << "total context events = " << context_count << '\n';
}
