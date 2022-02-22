#include <vull/support/StringBuilder.hh>

#include <vull/support/Array.hh>
#include <vull/support/Span.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Vector.hh>

#include <stdint.h>
#include <stdio.h>

namespace vull {

void StringBuilder::append_single(float arg) {
    // TODO: Own implementation.
    Array<char, 20> buf{};
    int length = snprintf(buf.data(), buf.size(), "%f", arg); // NOLINT
    m_buffer.extend(StringView(buf.data(), static_cast<size_t>(length)));
}

void StringBuilder::append_single(size_t arg) {
    Array<char, 20> buf{};
    uint8_t len = 0;
    do {
        auto digit = static_cast<char>(arg % 10);
        buf[len++] = '0' + digit; // NOLINT
        arg /= 10;
    } while (arg > 0);
    for (uint8_t i = len; i > 0; i--) {
        m_buffer.push(buf[i - 1]);
    }
}

void StringBuilder::append_single(StringView arg) {
    m_buffer.extend(arg);
}

String StringBuilder::build() {
    m_buffer.push('\0');
    auto buffer = m_buffer.take_all();
    return String::move_raw(buffer.data(), buffer.size() - 1);
}

String StringBuilder::build_copy() const {
    return String::copy_raw(m_buffer.data(), m_buffer.size());
}

} // namespace vull
