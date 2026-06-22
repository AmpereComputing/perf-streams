/*
 * Copyright (c) 2026, Ampere Computing LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdlib>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fstream>
#include <iostream>
#include <list>
#include <memory>
#include <regex>
#include <string>
#include <utility>
#include <vector>

namespace perf_streams::testing {

inline std::list<std::string> file_to_list(std::istream& in)
{
    static std::regex const ws{"^[ \t]*"};

    std::list<std::string> contents;
    std::string line;

    while (std::getline(in, line)) {
        line = std::regex_replace(line, ws, "");

        auto comment = line.find('#');

        if (comment != std::string::npos)
            line.erase(comment);

        if (!line.empty())
            contents.push_back(line);
    }

    return contents;
}

inline std::pair<std::string, std::string> parse_args(int argc, const char** argv)
{
    std::vector<std::string> args(&argv[1], &argv[argc]);
    if (args.size() < 2) {
        fmt::print(std::cerr, "usage: {} <input file> <output file>\n", argv[0]);
        exit(1);
    }

    return {args[0], args[1]};
}

inline std::pair<std::list<std::string>, std::string> parse_args_and_input(int argc, const char** argv)
{
    auto [input_file, output_file] = parse_args(argc, argv);
    std::ifstream in{input_file};
    return {file_to_list(in), output_file};
}

template<typename STREAM>
std::pair<std::list<std::string>, std::unique_ptr<STREAM>> parse_args_input_and_construct(int argc, const char** argv)
{
    auto [lines, output_file] = parse_args_and_input(argc, argv);
    return {lines, std::make_unique<STREAM>(output_file)};
}

} // namespace perf_streams::testing
