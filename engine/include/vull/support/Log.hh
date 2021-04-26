#pragma once

#include <cstdio>
#include <mutex>

class Log {
    struct Colour {
        Colour(int r, int g, int b) { std::printf("\x1b[38;2;%d;%d;%dm", r, g, b); }
        Colour(const Colour &) = delete;
        Colour(Colour &&) = delete;
        ~Colour() { std::puts("\x1b[0m"); }

        Colour &operator=(const Colour &) = delete;
        Colour &operator=(Colour &&) = delete;
    };

public:
    // NOLINTNEXTLINE
    static std::mutex s_log_lock;

    template <typename... Args>
    static void trace(const char *component, const char *fmt, const Args &...args) {
        std::scoped_lock lock(s_log_lock);
        Colour colour(70, 130, 180);
        std::printf("TRACE [%s] ", component);
        if constexpr (sizeof...(args) != 0) {
            std::printf(fmt, args...);
        } else {
            std::printf("%s", fmt);
        }
    }

    template <typename... Args>
    static void debug(const char *component, const char *fmt, const Args &...args) {
        std::scoped_lock lock(s_log_lock);
        Colour colour(100, 149, 237);
        std::printf("DEBUG [%s] ", component);
        if constexpr (sizeof...(args) != 0) {
            std::printf(fmt, args...);
        } else {
            std::printf("%s", fmt);
        }
    }

    template <typename... Args>
    static void info(const char *component, const char *fmt, const Args &...args) {
        std::scoped_lock lock(s_log_lock);
        Colour colour(224, 255, 255);
        std::printf("INFO  [%s] ", component);
        if constexpr (sizeof...(args) != 0) {
            std::printf(fmt, args...);
        } else {
            std::printf("%s", fmt);
        }
    }

    template <typename... Args>
    static void warn(const char *component, const char *fmt, const Args &...args) {
        std::scoped_lock lock(s_log_lock);
        Colour colour(255, 255, 0);
        std::printf("WARN  [%s] ", component);
        if constexpr (sizeof...(args) != 0) {
            std::printf(fmt, args...);
        } else {
            std::printf("%s", fmt);
        }
    }

    template <typename... Args>
    static void error(const char *component, const char *fmt, const Args &...args) {
        std::scoped_lock lock(s_log_lock);
        Colour colour(255, 69, 0);
        std::printf("ERROR [%s] ", component);
        if constexpr (sizeof...(args) != 0) {
            std::printf(fmt, args...);
        } else {
            std::printf("%s", fmt);
        }
    }
};
