#pragma once

#include <vull/container/vector.hh>
#include <vull/shaderc/token.hh>
#include <vull/support/string.hh>
#include <vull/support/utility.hh>

namespace vull::shaderc {

class ErrorMessage {
public:
    enum class Kind {
        Error,
        Note,
        NoteNoLine,
    };

private:
    Token m_token;
    String m_text;
    Kind m_kind;

public:
    ErrorMessage(Kind kind, const Token &token, String &&text)
        : m_token(token), m_text(vull::move(text)), m_kind(kind) {}

    const Token &token() const { return m_token; }
    const String &text() const { return m_text; }
    Kind kind() const { return m_kind; }
};

class Error {
    // TODO(small-vector)
    Vector<ErrorMessage> m_messages;

public:
    Error() = default;
    Error(const Error &other) { m_messages.extend(other.m_messages); }
    Error(Error &&) = default;
    ~Error() = default;

    Error &operator=(const Error &) = delete;
    Error &operator=(Error &&) = delete;

    void add_error(const Token &token, String &&message);
    void add_note(const Token &token, String &&message);
    void add_note_no_line(const Token &token, String &&message);

    const Vector<ErrorMessage> &messages() const { return m_messages; }
};

} // namespace vull::shaderc
