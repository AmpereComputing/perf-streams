/*
 * Copyright (c) 2026, Ampere Computing LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "event_stream/event.h"
#include "event_stream/event_stream.h"

#include <concepts>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace perf_streams::event_stream {

template<typename T>
std::pair<bool, EnumMappingType> event_data_definition_enumerations()
{
    return {true, {}};
}

template<typename T>
struct EventArgBase
{
    using value_type = T;
    using data_value_type = T;
    DataType data_type;
    T data_value;
};

template<typename T>
struct EventArg : EventArgBase<T>
{
    explicit operator bool() const { return true; }
    const T& value() const { return this->data_value; }
};

template<typename T>
struct EventArg<std::optional<T>> : EventArgBase<std::optional<T>>
{
    using data_value_type = T;
    explicit operator bool() const { return static_cast<bool>(this->data_value); }
    const T& value() const { return *this->data_value; }
};

template<typename T>
EventArg<T> to_event_arg(EventArg<T> x)
{
    return x;
}

template<typename T>
struct EventArgs
{
    static auto make(T x) { return to_event_arg(x); }
};

template<typename T>
struct EventArgs<EventArg<T>>
{
    static auto make(EventArg<T> x) { return x; }
};

template<typename X, typename Y>
struct EventArgs<std::pair<X, Y>>
{
    static auto make(std::pair<X, Y> arg) { return std::make_tuple(to_event_arg(arg.first), to_event_arg(arg.second)); }
};

template<typename... Ts>
struct EventArgs<std::tuple<Ts...>>
{
    static auto make(std::tuple<Ts...> arg)
    {
        return std::apply([](const auto&... args) { return std::make_tuple(to_event_arg(args)...); }, arg);
    }
};

template<typename T>
auto to_event_args(T&& x)
{
    return EventArgs<std::remove_cvref_t<T>>::make(std::forward<T>(x));
}

namespace detail {

template<class T>
concept enum_signed_integral = std::is_enum_v<T> && std::is_signed_v<std::underlying_type_t<T>>;

template<class T>
concept enum_unsigned_integral = std::is_enum_v<T> && std::is_unsigned_v<std::underlying_type_t<T>>;

inline void add_data_arg(EventStream& stream, EventHandle eh, DataType data_type, std::signed_integral auto data_value)
{
    return stream.add_int_data(eh, data_type, data_value);
}

inline void add_data_arg(EventStream& stream, EventHandle eh, DataType data_type, enum_signed_integral auto data_value)
{
    return stream.add_int_data(eh, data_type, std::to_underlying(data_value));
}

inline void add_data_arg(EventStream& stream,
                         EventHandle eh,
                         DataType data_type,
                         enum_unsigned_integral auto data_value)
{
    return stream.add_uint_data(eh, data_type, std::to_underlying(data_value));
}

inline void add_data_arg(EventStream& stream,
                         EventHandle eh,
                         DataType data_type,
                         std::unsigned_integral auto data_value)
{
    return stream.add_uint_data(eh, data_type, data_value);
}

inline void add_data_arg(EventStream& stream, EventHandle eh, DataType data_type, const char* data_value)
{
    stream.add_string_data(eh, data_type, data_value);
}

inline void add_data_arg(EventStream& stream, EventHandle eh, DataType data_type, const std::string& data_value)
{
    stream.add_string_data(eh, data_type, data_value);
}

template<typename T>
struct arg_as_tuple
{
    static std::tuple<T> make(T&& arg) { return std::forward_as_tuple(arg); }
};

template<typename... Ts>
struct arg_as_tuple<std::tuple<Ts...>>
{
    static std::tuple<Ts...> make(std::tuple<Ts...> arg) { return arg; }
};

template<typename... Ts>
using ArgPack = std::tuple<EventArg<Ts>...>;

template<typename... Ts>
auto make_arg_pack(Ts&&... args)
{
    return std::tuple_cat(
        arg_as_tuple<decltype(to_event_args(std::forward<Ts>(args)))>::make(to_event_args(std::forward<Ts>(args)))...);
}

template<int N>
struct arg_iter
{
    template<typename... Ts>
    static void add_data(EventStream& stream, EventHandle eh, const ArgPack<Ts...>& args)
    {
        arg_iter<N - 1>::add_data(stream, eh, args);
        const auto& arg = std::get<N - 1>(args);
        if (static_cast<bool>(arg))
            add_data_arg(stream, eh, arg.data_type, arg.value());
    }
};

template<>
struct arg_iter<0>
{
    template<typename... Ts>
    static void add_data(EventStream& stream [[maybe_unused]],
                         EventHandle eh [[maybe_unused]],
                         const ArgPack<Ts...>& args [[maybe_unused]])
    {
    }
};

} // namespace detail

class EventDefinition
{
public:
    EventDefinition(EventStream& event_stream, const std::string& name, const std::string& description);

    const auto& name() const { return _name; }
    const auto& description() const { return _description; }
    EventType event_type() const { return definition; }

    void at(TimeType at_time);

    template<typename T, typename... Ts>
    void at(TimeType at_time, T&& first, Ts&&... rest)
    {
        if (enabled && event_stream->is_enabled()) {
            auto args = detail::make_arg_pack(std::forward<T>(first), std::forward<Ts>(rest)...);
            auto* eh = event_stream->open_event(definition, at_time);
            detail::arg_iter<std::tuple_size_v<decltype(args)>>::add_data(*event_stream, eh, args);
            event_stream->close_event(eh);
        }
    }

    bool is_enabled() const { return enabled; }

    void enable() { enabled = true; }
    void disable() { enabled = false; }

    static void enable_all();
    static void disable_all();

protected:
    std::string _name;
    std::string _description;
    EventStream* event_stream;
    EventType definition;
    bool enabled{true};
};

template<typename T>
class EventDataDefinition
{
public:
    EventDataDefinition(EventStream& event_stream, const std::string& name, const std::string& description)
        : _name(name), _description(description), definition(make_type(event_stream, name, description))
    {
    }

    DataType data_type() const { return definition; }

    EventArg<T> operator()(T val) { return {definition, val}; }
    EventArg<std::optional<T>> optional(std::optional<T> val) { return {definition, val}; }

    EventArg<std::optional<T>> if_set(T val) { return {definition, val ? std::optional<T>(val) : std::nullopt}; }
    EventArg<std::optional<T>> if_set(T val, bool pred)
    {
        return {definition, pred ? std::optional<T>(val) : std::nullopt};
    }

protected:
    static DataType make_type(EventStream& event_stream, const std::string& name, const std::string& description)
    {
        if constexpr (std::is_enum_v<T>) {
            return make_enum_type<T>(event_stream, name, description);
        } else {
            return make_plain_type(event_stream, name, description);
        }
    }

    static DataType make_plain_type(EventStream& event_stream, const std::string& name, const std::string& description)
    {
        return event_stream.define_data(name, description);
    }

    static DataType make_anonymous_enum_type(EventStream& event_stream,
                                             const std::string& name,
                                             const std::string& description,
                                             const EnumMappingType& enumerations)
    {
        return event_stream.define_data_with_anonymous_enum(name, description, enumerations);
    }

    static DataType make_explicit_enum_type(EventStream& event_stream,
                                            const std::string& name,
                                            const std::string& description,
                                            bool complete,
                                            const EnumMappingType& enumerations)
    {
        if (complete)
            return event_stream.define_data_with_enum<T>(name, description, enumerations);
        return event_stream.define_data_with_enum<T>(name, description);
    }

    template<typename ET = T>
    static DataType make_enum_type(EventStream& event_stream, const std::string& name, const std::string& description)
    {
        auto [complete, enumerations] = event_data_definition_enumerations<ET>();
        if (complete && enumerations.empty())
            return make_plain_type(event_stream, name, description);
        return make_explicit_enum_type(event_stream, name, description, complete, enumerations);
    }

    DataType finalize_enum_type(EventStream& event_stream, const EnumMappingType& enumerations)
    {
        return event_stream.provide_enum<T>(enumerations);
    }

    static EnumMappingType enumeration_as_mapping(const T& enumeration)
    {
        EnumMappingType mapping;
        for (const auto& [value, str] : enumeration)
            mapping.emplace(static_cast<int64_t>(value), str);
        return mapping;
    }

    std::string _name;
    std::string _description;
    DataType definition;
};

} // namespace perf_streams::event_stream
