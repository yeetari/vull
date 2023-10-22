#pragma once

#include <vull/container/Array.hh>
#include <vull/platform/Timer.hh>
#include <vull/support/String.hh>
#include <vull/support/StringBuilder.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Utility.hh>

#include <stdint.h>

namespace vull {

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

void logln(StringView);
void open_log();
void close_log();

void print(StringView);
void println(StringView);
template <typename... Args>
void print(StringView fmt, Args &&...args) {
    print(vull::format(fmt, vull::forward<Args>(args)...));
}
template <typename... Args>
void println(StringView fmt, Args &&...args) {
    println(vull::format(fmt, vull::forward<Args>(args)...));
}

#define DEFINE_LOG_LEVEL(fn, lvl)                                                                                      \
    template <typename... Args>                                                                                        \
    void fn(StringView fmt, Args &&...args) {                                                                          \
        StringBuilder sb;                                                                                              \
        const uint64_t time = g_log_timer.elapsed_ns() / 1000000u;                                                     \
        sb.append("[{d5 }.{d3}] ", time / 1000u, time % 1000u);                                                        \
        sb.append(k_level_strings[static_cast<uint32_t>(lvl)]);                                                        \
        sb.append(fmt, vull::forward<Args>(args)...);                                                                  \
        logln(sb.build());                                                                                             \
    }

DEFINE_LOG_LEVEL(trace, LogLevel::Trace)
DEFINE_LOG_LEVEL(debug, LogLevel::Debug)
DEFINE_LOG_LEVEL(info, LogLevel::Info)
DEFINE_LOG_LEVEL(warn, LogLevel::Warn)
DEFINE_LOG_LEVEL(error, LogLevel::Error)
#undef DEFINE_LOG_LEVEL

} // namespace vull
