// Copyright (c) 2024-Present David Simoniants
// Distributed under the MIT License (https://opensource.org/licenses/MIT).
// See the LICENSE file in the top-level directory for more information.

#pragma once

#define SQUEEZE_LOG_FUNC_PREFIX ""
#ifdef NDEBUG
    #define SQUEEZE_LOG_PREFIX(format) format
#else
    #define SQUEEZE_LOG_PREFIX(format) "[{}{}(...)] " format, SQUEEZE_LOG_FUNC_PREFIX, __FUNCTION__
#endif

#define SQUEEZE_TRACE(...)      SQUEEZE_LOG(TRACE, __VA_ARGS__)
#define SQUEEZE_DEBUG(...)      SQUEEZE_LOG(DEBUG, __VA_ARGS__)
#define SQUEEZE_INFO(...)       SQUEEZE_LOG(INFO, __VA_ARGS__)
#define SQUEEZE_WARN(...)       SQUEEZE_LOG(WARN, __VA_ARGS__)
#define SQUEEZE_ERROR(...)      SQUEEZE_LOG(ERROR, __VA_ARGS__)
#define SQUEEZE_CRITICAL(...)   SQUEEZE_LOG(CRITICAL, __VA_ARGS__)

#ifdef SQUEEZE_USE_LOGGING

#ifndef SQUEEZE_LOG_LEVEL
    #ifdef NDEBUG
        #define SQUEEZE_LOG_LEVEL SPDLOG_LEVEL_INFO
    #else
        #define SQUEEZE_LOG_LEVEL SPDLOG_LEVEL_TRACE
    #endif
#endif

#define SPDLOG_ACTIVE_LEVEL SQUEEZE_LOG_LEVEL

#include <spdlog/spdlog.h>

#include <type_traits>
#include <string>

#define SQUEEZE_LOG(level, format, ...) SPDLOG_##level(SQUEEZE_LOG_PREFIX(format), ## __VA_ARGS__ )

namespace squeeze {

enum class LogLevel {
    Trace    = spdlog::level::trace,
    Debug    = spdlog::level::debug,
    Info     = spdlog::level::info,
    Warn     = spdlog::level::warn,
    Error    = spdlog::level::err,
    Critical = spdlog::level::critical,
    Off      = spdlog::level::off,
};

inline LogLevel get_log_level()
{
    return static_cast<LogLevel>(spdlog::get_level());
}

inline void set_log_level(LogLevel level)
{
    spdlog::set_level(static_cast<spdlog::level::level_enum>(
                static_cast<std::underlying_type_t<LogLevel>>(level)));
}

inline void init_logging()
{
#ifdef NDEBUG
    set_log_level(LogLevel::Info);
    spdlog::set_pattern("[%^%l%$] %v");
#else
    set_log_level(LogLevel::Trace);
    spdlog::set_pattern("[%^%l%$] [%s:%#] %v");
#endif
}

}

#else

#define SQUEEZE_LOG(level, format, ...)

namespace squeeze {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Critical,
    Off,
};

inline LogLevel get_log_level()
{
    return LogLevel::Off;
}

inline void set_log_level(LogLevel level)
{
}

inline void init_logging()
{
}

}

#endif /** SQUEEZE_USE_LOGGING */

