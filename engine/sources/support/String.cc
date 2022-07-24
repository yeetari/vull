#include <vull/support/String.hh>

#include <vull/support/Assert.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Utility.hh>

#include <string.h>

namespace vull {

String String::copy_raw(const char *data, size_t length) {
    String string(length);
    memcpy(string.m_data, data, length);
    return string;
}

String String::move_raw(char *data, size_t length) {
    VULL_ASSERT(data[length] == '\0');
    String string;
    string.m_data = data;
    string.m_length = length;
    return string;
}

String::String(size_t length) : m_length(length) {
    m_data = new char[length + 1];
    m_data[length] = '\0';
}

String::~String() {
    delete[] m_data;
}

String &String::operator=(String &&other) {
    m_data = exchange(other.m_data, nullptr);
    m_length = exchange(other.m_length, 0u);
    return *this;
}

bool String::operator==(const String &other) const {
    return StringView(m_data, m_length) == StringView(other.m_data, other.m_length);
}

} // namespace vull
