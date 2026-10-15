#pragma once

// 中文阅读说明
// 预编译头：集中引入本模块经常使用的游戏 SDK、日志和标准库。
// 阅读重点：它帮助编译器复用头文件处理结果，本身不承担点菜业务。

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
namespace logger = SKSE::log;
using namespace std::literals;
