/*
 * Copyright (c) 2026, Ampere Computing LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <capstone.h>
#include <cstdint>
#include <platform.h>
#include <string>

namespace perf_streams::disasm {

std::string disasm(std::uint32_t bytes, std::uint64_t pc);
void init();

} // namespace perf_streams::disasm
