#pragma once

#include <vull/support/StringView.hh>
#include <vull/support/Utility.hh>

#include <stddef.h>

namespace vull {

class String {
    char *m_data{nullptr};
    size_t m_length{0};

public:
    static String copy_raw(const char *data, size_t length);
    static String move_raw(char *data, size_t length);

    constexpr String() = default;
    explicit String(size_t length);
    String(const char *c_string) : String(copy_raw(c_string, __builtin_strlen(c_string))) {}
    String(const String &) = delete;
    String(String &&other) : m_data(exchange(other.m_data, nullptr)), m_length(exchange(other.m_length, 0u)) {}
    ~String();

    String &operator=(const String &) = delete;
    String &operator=(String &&);

    char *data() { return m_data; }
    const char *data() const { return m_data; }
    operator StringView() const { return {m_data, m_length}; }

    bool empty() const { return m_length == 0; }
    size_t length() const { return m_length; }
};

} // namespace vull
