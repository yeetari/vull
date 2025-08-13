#pragma once

#include <vull/container/vector.hh>
#include <vull/shaderc/source_location.hh>
#include <vull/support/string.hh>
#include <vull/support/utility.hh>

namespace vull::shaderc {

class Token;

class ErrorMessage {
public:
    enum class Kind {
        Error,
        Note,
        NoteNoLine,
    };

private:
    String m_text;
    SourceLocation m_source_location;
    Kind m_kind;

public:
    ErrorMessage(Kind kind, SourceLocation source_location, String &&text)
        : m_text(vull::move(text)), m_source_location(source_location), m_kind(kind) {}

    const String &text() const { return m_text; }
    SourceLocation source_location() const { return m_source_location; }
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

    void add_error(SourceLocation location, String &&message);
    void add_note(SourceLocation location, String &&message);
    void add_note_no_line(SourceLocation location, String &&message);
    void add_error(const Token &token, String &&message);
    void add_note(const Token &token, String &&message);
    void add_note_no_line(const Token &token, String &&message);

    const Vector<ErrorMessage> &messages() const { return m_messages; }
};

} // namespace vull::shaderc
