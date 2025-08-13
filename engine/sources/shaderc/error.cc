#include <vull/shaderc/error.hh>

#include <vull/container/vector.hh>
#include <vull/support/string.hh>
#include <vull/support/utility.hh>

namespace vull::shaderc {

class Token;

void Error::add_error(const Token &token, String &&message) {
    m_messages.emplace(ErrorMessage::Kind::Error, token, vull::move(message));
}

void Error::add_note(const Token &token, String &&message) {
    m_messages.emplace(ErrorMessage::Kind::Note, token, vull::move(message));
}

void Error::add_note_no_line(const Token &token, String &&message) {
    m_messages.emplace(ErrorMessage::Kind::NoteNoLine, token, vull::move(message));
}

} // namespace vull::shaderc
