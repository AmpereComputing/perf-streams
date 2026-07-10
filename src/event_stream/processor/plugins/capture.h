/*
 * Copyright (c) 2026, Ampere Computing LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "event_stream/processor/args.h"
#include "event_stream/processor/metric_table.h"
#include "event_stream/processor/plugin.h"
#include "event_stream/processor/processor_ifc.h"
#include "protobuf_utils/protobuf_stream.h"

#include <memory>

namespace perf_streams::event_stream::processor {

class CaptureBase : public Plugin
{
public:
    CaptureBase(ProcessorIfc& proc_ifc, Args& args);

    void define_event(const Definition& event_def) override;
    void define_value(const Definition& value_def) override;
    void define_enumeration(const Enumeration& enum_def) override;
    void report_parameter(const Parameter& parameter) override;
    void process_event(const Event& event) override;
    void start_simulation() override;
    void end_simulation() override;
    void report(MetricTableTimeSeries& ts) override;
    std::set<Phase> phases() const override { return {Phase::DEFINITIONS, Phase::PARAMETERS, Phase::EVENTS}; }

    static const char* category() { return "(ES output)"; }

protected:
    std::unique_ptr<protobuf_utils::ProtobufStreamWriter> output_stream;

    bool logging{true};

    int num_event_defs{0};
    int num_value_defs{0};
    int num_enum_defs{0};
    int num_parameters{0};
    int num_events{0};
};

} // namespace perf_streams::event_stream::processor
