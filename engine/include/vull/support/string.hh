#pragma once

#include <vull/support/hash.hh>
#include <vull/support/string_view.hh>
#include <vull/support/utility.hh>
// IWYU pragma: no_forward_declare vull::Hash

#include <stddef.h>

namespace vull {

class String {
    char *m_data{nullptr};
    size_t m_length{0};

public:
    static String copy_raw(const char *data, size_t length);
    static String move_raw(char *data, size_t length);
    static String repeated(char ch, size_t length);

    constexpr String() = default;
    explicit String(size_t length);
    String(const char *c_string) : String(copy_raw(c_string, __builtin_strlen(c_string))) {}
    String(StringView view) : String(copy_raw(view.data(), view.length())) {}
    String(const String &other) : String(copy_raw(other.m_data, other.m_length)) {}
    String(String &&other)
        : m_data(vull::exchange(other.m_data, nullptr)), m_length(vull::exchange(other.m_length, 0u)) {}
    ~String();

    String &operator=(const String &) = delete;
    String &operator=(String &&);

    int compare(StringView other) const;

    bool starts_with(char ch) const;
    bool ends_with(char ch) const;

    bool starts_with(StringView other) const;
    bool ends_with(StringView other) const;

    char *begin() const { return m_data; }
    char *end() const { return m_data + m_length; }

    char *data() { return m_data; }
    const char *data() const { return m_data; }

    char &operator[](size_t index) { return m_data[index]; }
    const char &operator[](size_t index) const { return m_data[index]; }

    operator StringView() const { return view(); }
    StringView view() const { return {m_data, m_length}; }

    char *disown() { return vull::exchange(m_data, nullptr); }

    bool empty() const { return m_length == 0; }
    size_t length() const { return m_length; }
};

template <>
struct Hash<String> {
    hash_t operator()(const String &string) const { return hash_of(string.view()); }
    hash_t operator()(const String &string, hash_t seed) const { return hash_of(string.view(), seed); }
};

} // namespace vull
