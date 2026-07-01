/*
 * Copyright (c) 2026, Ampere Computing LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <fmt/format.h>
#include <list>
#include <sstream>
#include <stdexcept>
#include <string>

namespace perf_streams::event_stream::processor {

class Args
{
public:
    void push(const std::string& arg) { arg_list.push_back(arg); }
    void push(const char* arg) { arg_list.emplace_back(arg); }

    bool pop(const char* cli_switch);

    template<typename T>
    bool pop(const char* cli_switch, T& arg);

    template<typename T>
    bool pop(T& positional_arg);

    template<typename T>
    bool pop_any(T& any_arg);

    const std::string& front() const { return arg_list.front(); }

    template<typename Transform>
    void transform(Transform transform_func);

    void done();

    explicit operator bool() const { return !arg_list.empty(); }

    static bool matches_switch(const std::string& arg, std::string cli_switch);

    auto begin() const { return arg_list.cbegin(); }
    auto end() const { return arg_list.cend(); }

private:
    std::list<std::string> arg_list;
};

namespace detail {

template<typename T>
struct from_string
{
    static T convert(const std::string& s)
    {
        std::stringstream ss{s};
        T val;
        ss >> val;
        return val;
    }
};

template<>
struct from_string<std::string>
{
    static std::string convert(const std::string& s) { return s; }
};

} // namespace detail

template<typename T>
bool Args::pop(const char* cli_switch, T& arg)
{
    if (arg_list.size() >= 2 && matches_switch(arg_list.front(), cli_switch)) {
        arg_list.pop_front();
        arg = detail::from_string<T>::convert(arg_list.front());
        arg_list.pop_front();
        return true;
    } else if (arg_list.size() == 1 && arg_list.front() == cli_switch) {
        throw std::runtime_error{fmt::format("{} expects an argument", cli_switch)};
    }

    return false;
}

template<typename T>
bool Args::pop(T& positional_arg)
{
    if (!arg_list.empty()) {
        auto& arg = arg_list.front();
        if (!arg.empty() && (arg[0] != '-' || arg.size() == 1)) {
            positional_arg = detail::from_string<T>::convert(arg);
            arg_list.pop_front();
            return true;
        }
    }

    return false;
}

template<typename T>
bool Args::pop_any(T& any_arg)
{
    if (!arg_list.empty()) {
        any_arg = detail::from_string<T>::convert(arg_list.front());
        arg_list.pop_front();
        return true;
    }

    return false;
}

template<typename Transform>
void Args::transform(Transform transform_func)
{
    for (auto& arg : arg_list)
        arg = transform_func(arg);
}

} // namespace perf_streams::event_stream::processor
