#include <vull/support/string.hh>

#include <vull/support/assert.hh>
#include <vull/support/string_view.hh>
#include <vull/support/utility.hh>

#include <string.h>

namespace vull {

String String::copy_raw(const char *data, size_t length) {
    if (data == nullptr || length == 0) {
        return {};
    }
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

String String::repeated(char ch, size_t length) {
    String string(length);
    memset(string.m_data, ch, length);
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
    String moved(vull::move(other));
    vull::swap(m_data, moved.m_data);
    vull::swap(m_length, moved.m_length);
    return *this;
}

int String::compare(StringView other) const {
    return view().compare(other);
}

bool String::starts_with(char ch) const {
    return view().starts_with(ch);
}

bool String::ends_with(char ch) const {
    return view().ends_with(ch);
}

bool String::starts_with(StringView other) const {
    return view().starts_with(other);
}

bool String::ends_with(StringView other) const {
    return view().ends_with(other);
}

bool String::operator==(const String &other) const {
    return StringView(m_data, m_length) == StringView(other.m_data, other.m_length);
}

} // namespace vull
