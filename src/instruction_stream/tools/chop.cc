// Copyright (c) 2026, Ampere Computing LLC
// SPDX-License-Identifier: BSD-3-Clause

#include "instruction_stream/instruction_stream.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// FIXME: remove using namespace
using namespace std::literals;
using namespace perf_streams::instruction_stream;

std::map<uint64_t, uint64_t> mem_image;

struct Options
{
    bool force{false};
    uint64_t wait_instructions{0};
    uint64_t warmup_instructions{5000};
    uint64_t measured_instructions{250000};
    uint64_t cooldown_instructions{5000};
    bool trim_memory{false};
};

template<typename T>
void get_option(T& opt_val, const std::string& argument)
{
    std::istringstream iss{argument};
    iss >> opt_val;
}

void print_help_and_exit()
{
    std::cerr << "Usage: is_chop [options] <input file> <output file>\n";
    std::cerr << "Options:\n";
    std::cerr << "  --wait         Number of instructions to wait (skip) before starting\n";
    std::cerr << "  --warmup       Number of instructions to warm up for\n";
    std::cerr << "  --measure      Number of instructions to measure\n";
    std::cerr << "  --cooldown     Number of instructions to cool down before stopping\n";
    std::cerr << "  --trim-memory  Only keep memory records needed by the reduced trace (does not support PRFM)\n\n";
    std::cerr << "  --force        Overwrite existing output file if present\n";

    exit(1);
}

std::vector<std::string> parse_arguments(std::vector<std::string> arguments, Options& options)
{
    std::vector<std::string> positional_arguments;

    try {
        for (size_t i = 0; i < arguments.size(); i++) {
            if ("--wait"s == arguments[i])
                get_option(options.wait_instructions, arguments.at(++i));
            else if ("--warmup"s == arguments[i])
                get_option(options.warmup_instructions, arguments.at(++i));
            else if ("--measure"s == arguments[i])
                get_option(options.measured_instructions, arguments.at(++i));
            else if ("--cooldown"s == arguments[i])
                get_option(options.cooldown_instructions, arguments.at(++i));
            else if ("--trim-memory"s == arguments[i])
                options.trim_memory = true;
            else if ("--force"s == arguments[i])
                options.force = true;
            else if ("--help"s == arguments[i] || "-h"s == arguments[i])
                print_help_and_exit();
            else if (arguments[i][0] == '-')
                throw std::runtime_error{"unrecognized option: "s + arguments[i]};
            else
                positional_arguments.push_back(arguments[i]);
        }
    } catch (std::out_of_range&) {
        std::cerr << "missing argument value" << '\n';
        print_help_and_exit();
    } catch (std::runtime_error& err) {
        std::cerr << err.what() << '\n';
        print_help_and_exit();
    }

    return positional_arguments;
}

int main(int argc, char** argv)
{
    Options options;

    auto files = parse_arguments(std::vector<std::string>(argv + 1, argv + argc), options);

    if (files.size() != 2)
        print_help_and_exit();

    std::filesystem::path const input_file_path(files[0]);
    std::filesystem::path const output_file_path(files[1]);

    auto reader = std::make_unique<InstructionStreamReader>(input_file_path);
    InstructionStreamWriter writer(output_file_path, reader->features(), options.force);
    Event event;
    uint64_t instruction_count{0};
    auto total_instructions = options.wait_instructions + options.warmup_instructions + options.measured_instructions
                              + options.cooldown_instructions;

    if (reader->version() < 2) {
        std::cerr << "Error: Instruction stream version < 2" << '\n';
        exit(1);
    }

    std::cout << "Chopping " << input_file_path << " to " << options.measured_instructions << " instructions with "
              << options.warmup_instructions << " instructions of warmup." << '\n';
    if (options.trim_memory)
        std::cout << "Trimming initial memory image to only addresses needed by output instructions\n";
    else
        std::cout << "Keeping all initial memory records\n";

    if (options.trim_memory) {
        // First pass -- record all 4k pages touched by the instructions which will be output
        while (reader->read(event) && instruction_count < total_instructions) {
            // Keep track of all the memory image we've seen so we can look up PTE values
            if (event.has_memory())
                mem_image[event.memory().address().physical_address()] = *((uint64_t*)event.memory().value().data());

            event.Clear();
        }

        // Set up for the second pass
        reader = std::make_unique<InstructionStreamReader>(input_file_path);
        instruction_count = 0;
    }

    bool started{false}, stopped{false};
    while (reader->read(event) && instruction_count < total_instructions) {
        // Don't write out start/stop_measurement controls, we're changing those
        if (event.has_control()
            && (event.control().type() == Control::START_MEASUREMENT
                || event.control().type() == Control::STOP_MEASUREMENT))
            goto next_event;

        if (event.has_instruction()) {
            instruction_count++;

            if (instruction_count <= options.wait_instructions)
                goto next_event;

            if (!started && instruction_count > options.wait_instructions + options.warmup_instructions) {
                Event control_event;
                Control control;
                control.set_type(Control::START_MEASUREMENT);
                control_event.mutable_control()->CopyFrom(control);
                writer.write(control_event);
                started = true;
            }
        }

        writer.write(event);

        if (event.has_instruction() && !stopped
            && instruction_count
                   >= options.wait_instructions + options.warmup_instructions + options.measured_instructions)
        {
            Event control_event;
            Control control;
            control.set_type(Control::STOP_MEASUREMENT);
            control_event.mutable_control()->CopyFrom(control);
            writer.write(control_event);
            stopped = true;
        }

    next_event:
        event.Clear();
    }

    writer.finalize();

    if (instruction_count < options.wait_instructions + options.warmup_instructions + options.measured_instructions
                                + options.cooldown_instructions)
    {
        std::cerr << "Error: Did not successfully read enough instructions from the input stream" << '\n';
        exit(1);
    }

    return 0;
}
