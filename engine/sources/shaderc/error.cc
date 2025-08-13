#include <vull/shaderc/error.hh>

#include <vull/container/vector.hh>
#include <vull/shaderc/source_location.hh>
#include <vull/shaderc/token.hh>
#include <vull/support/string.hh>
#include <vull/support/utility.hh>

namespace vull::shaderc {

void Error::add_error(SourceLocation location, String &&message) {
    m_messages.emplace(ErrorMessage::Kind::Error, location, vull::move(message));
}

void Error::add_note(SourceLocation location, String &&message) {
    m_messages.emplace(ErrorMessage::Kind::Note, location, vull::move(message));
}

void Error::add_note_no_line(SourceLocation location, String &&message) {
    m_messages.emplace(ErrorMessage::Kind::NoteNoLine, location, vull::move(message));
}

void Error::add_error(const Token &token, String &&message) {
    add_error(token.location(), vull::move(message));
}

void Error::add_note(const Token &token, String &&message) {
    add_note(token.location(), vull::move(message));
}

void Error::add_note_no_line(const Token &token, String &&message) {
    add_note_no_line(token.location(), vull::move(message));
}

} // namespace vull::shaderc
