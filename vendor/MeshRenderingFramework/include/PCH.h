#pragma once

#include "spdlog/sinks/basic_file_sink.h"

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

namespace logger = SKSE::log;
using namespace std::literals;

using FormID = RE::FormID;
using RefID = RE::FormID;

const RefID player_refid = 20;

#define IF_FIND(array, value, it) \
    auto it = array.find(value);  \
    if (it != array.end())
