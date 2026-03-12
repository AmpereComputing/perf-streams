// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "instruction_stream/instruction_stream.h"
#include "instruction_stream/instruction_stream.pb.h"
#include "testing/generate_stream.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <ios>
#include <list>
#include <regex>
#include <sstream>
#include <string>
#include <utility>

using namespace perf_streams::instruction_stream;

Features text_to_features(const std::list<std::string>& lines)
{
    static const std::regex features_header{R"(.*feature support:$)", std::regex_constants::icase};
    static const std::regex features_value{R"(^(.*?) +: +(true|false)$)", std::regex_constants::icase};
    using feature_setter = decltype(&Features::set_memory_translation);
    static const std::array<std::pair<std::regex, feature_setter>, 10> feature_mapping = {{
        {std::regex(R"(memory translation)", std::regex_constants::icase), &Features::set_memory_translation},
        {std::regex(R"((div(ide)|sqrt).* source registers)", std::regex_constants::icase),
         &Features::set_divide_sqrt_registers},
        {std::regex(R"(cache maintenance source registers)", std::regex_constants::icase),
         &Features::set_cache_maintenance_registers},
        {std::regex(R"(prefetch source registers)", std::regex_constants::icase), &Features::set_prefetch_registers},
        {std::regex(R"(destination registers)", std::regex_constants::icase), &Features::set_all_destination_registers},
        {std::regex(R"(memory values)", std::regex_constants::icase), &Features::set_memory_values},
        {std::regex(R"(predicated memops)", std::regex_constants::icase), &Features::set_sve_predicated_memops},
        {std::regex(R"(predicate registers)", std::regex_constants::icase), &Features::set_sve_predicate_registers},
        {std::regex(R"(memop size)", std::regex_constants::icase), &Features::set_memop_size},
        {std::regex(R"(nzcv flags)", std::regex_constants::icase), &Features::set_nzcv_flags_on_instructions},
    }};

    Features features;
    bool within_header = false;
    for (const auto& line : lines) {
        if (within_header) {
            std::smatch m;
            if (std::regex_match(line, m, features_value)) {
                bool feature_set{false};
                std::istringstream(m[2].str()) >> std::boolalpha >> feature_set;
                for (const auto& [feature, setter] : feature_mapping) {
                    if (std::regex_search(m[1].str(), feature)) {
                        (features.*setter)(feature_set);
                        break;
                    }
                }
            } else {
                break;
            }
        } else if (!std::regex_search(line, features_header))
            continue;
    }
    return features;
}

uint64_t text_to_address(const std::string& text)
{
    return std::stoull(text, nullptr, 16);
}

std::string text_to_bytes(const std::string& text, size_t size = 0)
{
    constexpr size_t chars = (sizeof(long long) * 2);
    std::string bytes;
    for (size_t i = text.size(); i > 0; i -= chars) {
        auto b = text.substr(i > chars ? i - chars : 0, std::min(chars, i));
        auto n = std::stoull(b, nullptr, 16);
        bytes.append(reinterpret_cast<const char*>(&n), sizeof(long long));
    }

    if (size && bytes.size() < size)
        bytes.append(size - bytes.size(), '\0');

    return bytes;
}

std::list<Memory> text_to_memory(const std::list<std::string>& lines)
{
    static const std::regex location_re{
        R"(^Memory: (va|virtual address): 0x([a-f0-9]+),? (pa|physical address): 0x([a-f0-9]+),? size: (\d+),? value: 0x([a-f0-9]+).*$)",
        std::regex_constants::icase};
    std::list<Memory> memory;
    for (const auto& line : lines) {
        std::smatch m;
        if (std::regex_match(line, m, location_re)) {
            auto& location = memory.emplace_back();
            location.mutable_address()->set_virtual_address(text_to_address(m[2].str()));
            location.mutable_address()->set_physical_address(text_to_address(m[4].str()));
            location.set_value(text_to_bytes(m[6].str(), std::stoul(m[5].str())));
        }
    }
    return memory;
}

std::list<Instruction> text_to_instructions(const std::list<std::string>& lines)
{
    static const std::regex instruction_start{R"(^Instruction: .*?pc: 0x([a-f0-9]+),? op: 0x([a-f0-9]+) *(.*)$)",
                                              std::regex_constants::icase};
    static const std::regex memop{
        R"((va|virtual address)\[\d+\]: 0x([a-f0-9]+),? (pa|physical address)\[\d+\]: 0x([a-f0-9]+),? bytes: 0x([a-f0-9]+))",
        std::regex_constants::icase};

    std::list<Instruction> instructions;
    for (const auto& line : lines) {
        std::smatch m;
        if (std::regex_match(line, m, instruction_start)) {
            auto& instruction = instructions.emplace_back();
            instruction.mutable_program_counter()->set_virtual_address(text_to_address(m[2].str()));
            instruction.set_opcode(std::stoul(m[2].str(), nullptr, 16));

            auto other = m[3].str();
            for (auto it = std::sregex_iterator(other.begin(), other.end(), memop), eit = std::sregex_iterator();
                 it != eit;
                 ++it)
            {
                const auto& mm = *it;
                auto* memop = instruction.add_memop();
                memop->set_virtual_address(text_to_address(mm[2].str()));
                memop->set_physical_address(text_to_address(mm[4].str()));
                memop->set_bytes(text_to_address(mm[5].str()));
            }
        }
    }
    return instructions;
}

void generate(const std::list<Memory>& memory, const std::list<Instruction>& instructions, InstructionStreamWriter& is)
{
    for (const auto& location : memory)
        is.write(location);

    Control control;
    control.set_type(perf_streams::instruction_stream::Control_Type_START_MEASUREMENT);
    is.write(control);

    for (const auto& instruction : instructions)
        is.write(instruction);

    control.set_type(perf_streams::instruction_stream::Control_Type_STOP_MEASUREMENT);
    is.write(control);

    is.finalize();
}

int main(int argc, const char** argv)
{
    auto [lines, output_file] = perf_streams::testing::parse_args_and_input(argc, argv);
    auto features = text_to_features(lines);
    InstructionStreamWriter is(output_file, features);
    auto memory = text_to_memory(lines);
    auto instructions = text_to_instructions(lines);
    generate(memory, instructions, is);
    return EXIT_SUCCESS;
}
