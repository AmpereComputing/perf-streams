/*
 * Copyright (c) 2026, Ampere Computing LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "event_stream/event.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <variant>

namespace perf_streams::event_stream {

class EventDefinition;
class EventStreamForwarder;

using EventType = uint32_t;
using DataType = uint32_t;
using TimeType = uint64_t;
using EnumType = uint32_t;
using EnumMappingType = std::unordered_map<int64_t, std::string>;

class EventStream
{
public:
    struct using_global_definitions_t
    {};
    inline static using_global_definitions_t using_global_definitions{};

    EventStream()
        : owned_definitions(std::make_unique<Definitions>()),
          definitions(owned_definitions.get()),
          owned_enumerations(std::make_unique<Enumerations>()),
          enumerations(owned_enumerations.get())
    {
    }
    explicit EventStream(using_global_definitions_t hint)
        : definitions(&Definitions::instance()), enumerations(&Enumerations::instance())
    {
    }

    virtual ~EventStream() = default;

    virtual void post_event(EventType event_type, TimeType time) { close_event(open_event(event_type, time)); }
    virtual EventHandle open_event(EventType event_type, TimeType time) = 0;
    virtual void close_event(EventHandle event) = 0;

    virtual void add_int_data(EventHandle event, DataType event_data_type, std::int64_t value) = 0;
    virtual void add_uint_data(EventHandle event, DataType event_data_type, std::uint64_t value) = 0;
    virtual void add_string_data(EventHandle event, DataType event_data_type, const std::string& value) = 0;

    virtual void set_bool_parameter(const std::string& name, const std::string& description, bool value) = 0;
    virtual void set_int_parameter(const std::string& name, const std::string& description, std::int64_t value) = 0;
    virtual void set_uint_parameter(const std::string& name, const std::string& description, std::uint64_t value) = 0;
    virtual void set_double_parameter(const std::string& name, const std::string& description, double value) = 0;
    virtual void set_string_parameter(const std::string& name,
                                      const std::string& description,
                                      const std::string& value) = 0;
    virtual void set_json_parameter(const std::string& name,
                                    const std::string& description,
                                    const std::string& value) = 0;
    virtual void start_simulation() {}

    EventType define_event(EventDefinition& event);
    EventType define_event(const std::string& name, const std::string& description);
    DataType define_data(const std::string& name, const std::string& description);
    DataType define_data(const std::string& name, const std::string& description, EnumType enumeration);

    EnumType define_enum(size_t id, const EnumMappingType& enumerations);

    template<typename T>
    DataType define_data_with_enum(const std::string& name, const std::string& description)
    {
        auto enum_type = reserve_enum<T>();
        return define_data(name, description, enum_type);
    }

    template<typename T>
    DataType define_data_with_enum(const std::string& name,
                                   const std::string& description,
                                   const EnumMappingType& enumerations)
    {
        auto enum_type = define_enum<T>(enumerations);
        return define_data(name, description, enum_type);
    }

    EnumType define_data_with_anonymous_enum(const std::string& name,
                                             const std::string& description,
                                             const EnumMappingType& enumerations)
    {
        auto enum_type = define_anonymous_enum(enumerations);
        return define_data(name, description, enum_type);
    }

    template<typename T>
    EnumType provide_enum(const EnumMappingType& enumerations)
    {
        return provide_enum(typeid(T), enumerations);
    }

    virtual void enable() = 0;
    virtual void disable() = 0;
    virtual bool is_enabled() { return true; }

protected:
    EnumType define_anonymous_enum(const EnumMappingType& enumerations);
    template<typename T>
    EnumType reserve_enum()
    {
        return reserve_enum(typeid(T));
    }
    EnumType reserve_enum(std::type_index enum_type);
    template<typename T>
    EnumType define_enum(const EnumMappingType& enumerations)
    {
        return define_enum(typeid(T), enumerations);
    }
    EnumType define_enum(std::type_index enum_type, const EnumMappingType& enumerations);
    EnumType provide_enum(std::type_index enum_type, const EnumMappingType& enumerations);

    void catch_up(bool complete = false);

    virtual void finalize_event_definition(EventType event_type,
                                           const std::string& name,
                                           const std::string& description) = 0;
    virtual void finalize_data_definition(DataType data_type,
                                          const std::string& name,
                                          const std::string& description) = 0;
    virtual void finalize_data_definition(DataType data_type,
                                          const std::string& name,
                                          const std::string& description,
                                          EnumType enumeration [[maybe_unused]])
    {
        finalize_data_definition(data_type, name, description);
    }
    virtual void finalize_enum_definition(EnumType enum_type [[maybe_unused]],
                                          const EnumMappingType& enumerations [[maybe_unused]])
    {
    }

    class Definitions
    {
    public:
        static Definitions& instance();

        EventType define_event(EventDefinition& event);
        EventType define_event(const std::string& name, const std::string& description);
        DataType define_data(const std::string& name, const std::string& description);
        DataType define_data(const std::string& name, const std::string& description, EnumType enumeration);
        EventDefinition* get_mutable_event(EventType id)
        {
            auto ev = events.find(id);
            return ev == events.end() ? nullptr : ev->second;
        }
        const EventDefinition* get_event(EventType id) const
        {
            auto ev = events.find(id);
            return ev == events.end() ? nullptr : ev->second;
        }
        const auto& get_definition(EventType id) const { return definitions.at(id - 1); }

        bool caught_up(size_t index) const { return index >= definitions.size(); }
        size_t catch_up(EventStream& stream, size_t index);
        void clear();

        Definitions() = default;

    private:
        enum class DefType {
            EVENT,
            DATA
        };
        struct Definition
        {
            DefType type;
            std::string name;
            std::string description;
            std::optional<EnumType> enumeration;
            Definition(DefType type, const std::string& name, const std::string& description)
                : type(type), name(name), description(description)
            {
            }
            Definition(DefType type, const std::string& name, const std::string& description, EnumType enum_type)
                : type(type), name(name), description(description), enumeration(enum_type)
            {
            }
        };
        std::deque<Definition> definitions;
        std::mutex definitions_mtx;
        std::unordered_map<EventType, EventDefinition*> events;
    };

    class Enumerations
    {
    public:
        static Enumerations& instance();

        template<typename T>
        EnumType reserve_enum()
        {
            return reserve_enum(typeid(T));
        }
        EnumType reserve_enum(std::type_index enum_type);

        template<typename T>
        EnumType define_enum(const EnumMappingType& enumerations)
        {
            return define_enum(typeid(T), enumerations);
        }
        EnumType define_enum(std::type_index enum_type, const EnumMappingType& enumerations);
        EnumType define_enum(size_t id, const EnumMappingType& enumerations);

        template<typename T>
        EnumType provide_enum(const EnumMappingType& enumerations)
        {
            return provide_enum(typeid(T), enumerations);
        }
        EnumType provide_enum(std::type_index enum_type, const EnumMappingType& enumerations);

        bool caught_up(size_t index) const { return index >= enumerations.size(); }
        size_t catch_up(EventStream& stream, size_t index);
        void clear();

        Enumerations() = default;

    private:
        std::deque<EnumMappingType> enumerations;
        std::unordered_map<std::variant<std::type_index, size_t>, EnumType> enumeration_types;
        std::mutex enumerations_mtx;
    };

    std::unique_ptr<Definitions> owned_definitions;
    Definitions* definitions{nullptr};
    size_t definition_index{0};

    std::unique_ptr<Enumerations> owned_enumerations;
    Enumerations* enumerations{nullptr};
    size_t enumeration_index{0};

    friend class EventStreamBroadcast;
    friend class EventStreamForwarder;

public:
    static Definitions& global_definitions();
    static Enumerations& global_enumerations();
};

} // namespace perf_streams::event_stream
