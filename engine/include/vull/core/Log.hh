#pragma once

#include <vull/platform/Timer.hh>
#include <vull/support/String.hh>
#include <vull/support/StringBuilder.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Utility.hh>

#include <stdint.h>

namespace vull {

void logln(StringView);
void open_log();
void close_log();

bool log_colours_enabled();
void set_log_colours_enabled(bool log_colours_enabled);

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

#define DEFINE_LOG_LEVEL(fn, LEVEL_STRING, LEVEL_COLOUR)                                                               \
    template <typename... Args>                                                                                        \
    void fn(StringView fmt, Args &&...args) {                                                                          \
        extern Timer g_log_timer;                                                                                      \
                                                                                                                       \
        StringBuilder sb;                                                                                              \
        const uint64_t time = g_log_timer.elapsed_ns() / 1000000u;                                                     \
        if (log_colours_enabled()) {                                                                                   \
            sb.append("\x1b[37m");                                                                                     \
        }                                                                                                              \
        sb.append("[{d5 }.{d3}] ", time / 1000u, time % 1000u);                                                        \
        if (log_colours_enabled()) {                                                                                   \
            sb.append("\x1b[0m" LEVEL_COLOUR);                                                                         \
        }                                                                                                              \
        sb.append(LEVEL_STRING);                                                                                       \
        if (log_colours_enabled()) {                                                                                   \
            sb.append("\x1b[0m");                                                                                      \
        }                                                                                                              \
        sb.append(fmt, vull::forward<Args>(args)...);                                                                  \
        logln(sb.build());                                                                                             \
    }

DEFINE_LOG_LEVEL(trace, "TRACE ", "\x1b[35m")
DEFINE_LOG_LEVEL(debug, "DEBUG ", "\x1b[36m")
DEFINE_LOG_LEVEL(info, "INFO  ", "\x1b[32m")
DEFINE_LOG_LEVEL(warn, "WARN  ", "\x1b[1;33m")
DEFINE_LOG_LEVEL(error, "ERROR ", "\x1b[1;31m")
#undef DEFINE_LOG_LEVEL

} // namespace vull
