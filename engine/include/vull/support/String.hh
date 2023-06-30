#pragma once

#include <vull/support/Hash.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Utility.hh>
// IWYU pragma: no_forward_declare vull::Hash

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
    String(StringView view) : String(copy_raw(view.data(), view.length())) {}
    String(const String &other) : String(copy_raw(other.m_data, other.m_length)) {}
    String(String &&other) : m_data(exchange(other.m_data, nullptr)), m_length(exchange(other.m_length, 0u)) {}
    ~String();

    String &operator=(const String &) = delete;
    String &operator=(String &&);

    char *begin() const { return m_data; }
    char *end() const { return m_data + m_length; }

    char *data() { return m_data; }
    const char *data() const { return m_data; }

    char &operator[](size_t index) { return m_data[index]; }
    const char &operator[](size_t index) const { return m_data[index]; }

    bool ends_with(StringView end);

    operator StringView() const { return view(); }
    StringView view() const { return {m_data, m_length}; }

    char *disown() { return exchange(m_data, nullptr); }

    bool operator==(const String &other) const;
    bool empty() const { return m_length == 0; }
    size_t length() const { return m_length; }
};

template <>
struct Hash<String> {
    hash_t operator()(const String &string) const { return hash_of(string.view()); }
    hash_t operator()(const String &string, hash_t seed) const { return hash_of(string.view(), seed); }
};

} // namespace vull
