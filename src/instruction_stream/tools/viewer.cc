// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "instruction_stream/disasm.h"
#include "instruction_stream/instruction_stream.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <fcntl.h>
#include <fmt/base.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <ios>
#include <iostream>
#include <optional>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace perf_streams::instruction_stream;

static std::string nzcv_str(std::uint32_t nzcv)
{
    const std::uint32_t nibble = nzcv >> 28;
    char n = (nibble & 8) ? 'n' : '-';
    char z = (nibble & 4) ? 'z' : '-';
    char c = (nibble & 2) ? 'c' : '-';
    char v = (nibble & 1) ? 'v' : '-';
    return fmt::format("{}{}{}{}", n, z, c, v);
}

static void usage(char const* prog)
{
    constexpr const char* usage = R"("Usage {} [options] instruction-stream.is.xz

    -w, --exclude-warmup   do not output warmup instructions; only emit "measurement"
    -m, --skip-memory      do not output initial memory records before the first instruction
    -h, --help             this help message
)";

    fmt::print(usage, prog);
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv)
{
    bool include_warmup_instrs = true;
    bool skip_memory = false;

    std::optional<std::string> instruction_stream_fn = std::nullopt;
    std::deque<std::string> cmdline(argv + 1, argv + argc);
    while (!cmdline.empty()) {
        auto arg = cmdline.front();
        cmdline.pop_front();
        if (arg == "-h" || arg == "--help") {
            usage(argv[0]);
        } else if (arg == "-w" || arg == "--exclude-warmup") {
            include_warmup_instrs = false;
        } else if (arg == "-m" || arg == "--skip-memory") {
            skip_memory = true;
        } else {
            if (instruction_stream_fn.has_value()) {
                fmt::print(stderr, "Can only specify one instruction stream file\n");
                exit(EXIT_FAILURE);
            }
            instruction_stream_fn = arg;
        }
    }
    if (!instruction_stream_fn.has_value()) {
        fmt::print(stderr, "Need an instruction stream file name\n");
        exit(EXIT_FAILURE);
    }

    InstructionStreamReader reader(instruction_stream_fn.value());
    Event event;

    perf_streams::disasm::init();

    fmt::print("Instruction stream is version {} with the following feature support:\n", reader.version());
    struct str_feature_t
    {
        const char* str;
        bool present;
    };

    str_feature_t str_feature[] = {
        {"Memory translation collateral", reader.features().memory_translation()},
        {"Divide/SQRT source registers", reader.features().divide_sqrt_registers()},
        {"Cache maintenance source registers", reader.features().cache_maintenance_registers()},
        {"SW prefetch source registers", reader.features().prefetch_registers()},
        {"All destination registers", reader.features().all_destination_registers()},
        {"Memory values", reader.features().memory_values()},
        {"SVE predicated memops", reader.features().sve_predicated_memops()},
        {"SVE predicate registers", reader.features().sve_predicate_registers()},
        {"Memop size", reader.features().memop_size()},
        {"NZCV flags on instructions", reader.features().nzcv_flags_on_instructions()},
        {nullptr, false}};
    for (str_feature_t* p = str_feature; p->str; ++p) {
        fmt::print("  {:35s}: {}\n", p->str, p->present);
    }
    fmt::print("\n");

    long event_count = 0, instruction_count = 0, memop_count = 0, memory_count = 0, sysreg_count = 0, context_count = 0,
         control_count = 0;

    bool measurement_started = (reader.version() < 2) || include_warmup_instrs;
    if (!measurement_started)
        std::cout << "Ignoring instructions before START_MEASUREMENT event" << '\n';

    while (reader.read(event)) {
        event_count++;
        if (event.has_instruction()) {
            instruction_count++;
            const auto& instruction = event.instruction();
            if (measurement_started) {
                std::cout << "Instruction: inum: " << std::dec << instruction_count << " "
                          << "pc: 0x" << std::hex << instruction.program_counter().virtual_address() << " "
                          << "op: 0x" << instruction.opcode() << " "
                          << "dis: `"
                          << perf_streams::disasm::disasm(instruction.opcode(),
                                                          instruction.program_counter().virtual_address())
                          << "` ";
                if (instruction.memop_size() > 0) {
                    for (int i = 0; i < instruction.memop_size(); i++) {
                        std::cout << "virtual address[" << i << "]: 0x" << std::hex
                                  << instruction.memop(i).virtual_address() << " ";
                    }
                    for (int i = 0; i < instruction.memop_size(); i++) {
                        std::cout << "physical address[" << i << "]: 0x" << std::hex
                                  << instruction.memop(i).physical_address() << " ";
                        if (instruction.memop(i).predicated_off()) {
                            std::cout << "PREDOFF ";
                        }
                        std::cout << fmt::format("bytes={:d} ", instruction.memop(i).bytes());
                    }
                }
                // src regs
                if (instruction.register__size() > 0) {
                    for (int i = 0; i < instruction.register__size(); i++) {
                        if (instruction.register_(i).number() == 30) {
                            std::cout << "lr(x30)";
                        } else if (instruction.register_(i).number() == 31) {
                            std::cout << "sp";
                        } else {
                            std::cout << "x" << std::dec << instruction.register_(i).number();
                        }
                        std::cout << " -> 0x" << std::hex << instruction.register_(i).value() << " ";
                    }
                }
                if (instruction.vfp_register_size() > 0) {
                    for (int i = 0; i < instruction.vfp_register_size(); i++) {
                        std::cout << "v" << std::dec << instruction.vfp_register(i).number();
                        std::cout << " -> 0x" << std::hex << instruction.vfp_register(i).ms_value() << "_" << std::hex
                                  << instruction.vfp_register(i).ls_value() << " ";
                    }
                }
                if (instruction.src_pred_register_size() > 0) {
                    for (int i = 0; i < instruction.src_pred_register_size(); i++) {
                        std::cout << "p" << std::dec << instruction.src_pred_register(i).number();
                        std::cout << fmt::format(" -> 0x{:04x} ", instruction.src_pred_register(i).value());
                    }
                }
                if (instruction.has_src_nzcv()) {
                    const unsigned nzcv = instruction.src_nzcv() >> 28;
                    fmt::print("PSTATE.NZCV -> {} ", nzcv_str(nzcv));
                }

                // dest regs
                if (instruction.dst_register_size() > 0) {
                    for (int i = 0; i < instruction.dst_register_size(); i++) {
                        if (instruction.dst_register(i).number() == 30) {
                            std::cout << "lr(x30)";
                        } else if (instruction.dst_register(i).number() == 31) {
                            std::cout << "sp";
                        } else {
                            std::cout << "x" << std::dec << instruction.dst_register(i).number();
                        }
                        std::cout << " <- 0x" << std::hex << instruction.dst_register(i).value() << " ";
                    }
                }
                if (instruction.dst_vfp_register_size() > 0) {
                    for (int i = 0; i < instruction.dst_vfp_register_size(); i++) {
                        fmt::print("v{} <- {:016x}_{:016x} ",
                                   instruction.dst_vfp_register(i).number(),
                                   instruction.dst_vfp_register(i).ms_value(),
                                   instruction.dst_vfp_register(i).ls_value());
                    }
                }
                if (instruction.dst_pred_register_size() > 0) {
                    for (int i = 0; i < instruction.dst_pred_register_size(); i++) {
                        fmt::print("p{} <- 0x{:04x} ",
                                   instruction.dst_pred_register(i).number(),
                                   instruction.dst_pred_register(i).value());
                    }
                }
                if (instruction.has_dst_nzcv()) {
                    const unsigned nzcv = instruction.dst_nzcv();
                    fmt::print("PSTATE.NZCV <- {} ", nzcv_str(nzcv));
                }
                if (instruction.has_exception()) {
                    fmt::print("caused exception (ESR_EL1.EC = {:#b}) ", instruction.exception().exception_class());
                }
                std::cout << '\n';
            }
            memop_count += instruction.memop_size();
        } else if (event.has_memory()) {
            if (instruction_count > 0 || !skip_memory) {
                const auto& memory = event.memory();
                std::cout << "Memory: "
                          << "virtual address: 0x" << std::hex << memory.address().virtual_address() << " "
                          << "physical address: 0x" << std::hex << memory.address().physical_address() << " "
                          << "size: " << memory.value().size() << " "
                          << "value:";

                size_t size = memory.value().size();
                auto* value = (uint8_t*)memory.value().data();

                switch (size) {
                case 8:
                    std::cout << " 0x" << std::hex << *(uint64_t*)value;
                    break;
                case 4:
                    std::cout << " 0x" << std::hex << *(uint32_t*)value;
                    break;
                case 2:
                    std::cout << " 0x" << std::hex << *(uint16_t*)value;
                    break;
                case 1:
                    std::cout << " 0x" << std::hex << (uint16_t)*value;
                    break;
                default:
                    uint64_t data = 0;
                    size_t read_so_far = 0;
                    while (size) {
                        data |= (uint64_t)*value << read_so_far;
                        read_so_far += 8;
                        size--;
                        value++;
                    }
                    std::cout << " 0x" << std::hex << data;
                }
                std::cout << '\n';
            }
            memory_count++;
        } else if (event.has_sysreg()) {
            const auto& sysreg = event.sysreg();
            std::cout << "SysReg: "
                      << "op0: " << sysreg.op0() << " "
                      << "op1: " << sysreg.op1() << " "
                      << "crn: " << sysreg.crn() << " "
                      << "crm: " << sysreg.crm() << " "
                      << "op2: " << sysreg.op2() << " "
                      << "value: 0x" << std::hex << sysreg.value() << '\n';
            sysreg_count++;
        } else if (event.has_context()) {
            const auto& context = event.context();
            std::cout << "Context: "
                      << "type: " << Context::Type_Name(context.type()) << " "
                      << "value: " << context.value() << '\n';
            context_count++;
        } else if (event.has_control()) {
            const auto& control = event.control();
            std::cout << "Control: " << "type: " << Control::Type_Name(control.type()) << '\n';
            control_count++;
            if (control.type() == Control::START_MEASUREMENT) {
                if (!include_warmup_instrs)
                    std::cout << "Omitted " << std::dec << instruction_count
                              << " instructions before measurement began." << '\n';
                measurement_started = true;
            }
        }
        event.Clear();
    }

    std::cout << std::dec;
    std::cout << "instruction count = " << instruction_count << '\n';
    std::cout << "memop count = " << memop_count << '\n';
    std::cout << "memory count = " << memory_count << '\n';
    std::cout << "sysreg count = " << sysreg_count << '\n';
    std::cout << "context count = " << context_count << '\n';
    std::cout << "control count = " << control_count << '\n';
    std::cout << "total event count = " << event_count << '\n';
}
