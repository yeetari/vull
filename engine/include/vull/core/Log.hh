#pragma once

#include <vull/support/Array.hh>
#include <vull/support/StringBuilder.hh>
#include <vull/support/Timer.hh>
#include <vull/support/Utility.hh>

#include <stdint.h>

namespace vull {

class String;

extern Timer g_log_timer;

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
};

constexpr Array k_level_strings{
    "TRACE ", "DEBUG ", "INFO  ", "WARN  ", "ERROR ",
};

void log_raw(String &&message);
void log_close();

#define DEFINE_LOG_LEVEL(fn, lvl)                                                                                      \
    template <typename... Args>                                                                                        \
    void fn(const char *fmt, Args &&...args) {                                                                         \
        StringBuilder sb;                                                                                              \
        const uint64_t time = g_log_timer.elapsed_ns() / 1000000u;                                                     \
        sb.append("[{d5 }.{d3}] ", time / 1000u, time % 1000u);                                                        \
        sb.append(k_level_strings[static_cast<uint32_t>(lvl)]);                                                        \
        sb.append(fmt, vull::forward<Args>(args)...);                                                                  \
        log_raw(sb.build());                                                                                           \
    }

DEFINE_LOG_LEVEL(trace, LogLevel::Trace)
DEFINE_LOG_LEVEL(debug, LogLevel::Debug)
DEFINE_LOG_LEVEL(info, LogLevel::Info)
DEFINE_LOG_LEVEL(warn, LogLevel::Warn)
DEFINE_LOG_LEVEL(error, LogLevel::Error)
#undef DEFINE_LOG_LEVEL

} // namespace vull
