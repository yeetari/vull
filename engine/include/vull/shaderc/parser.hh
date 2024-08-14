#pragma once

#include <vull/container/hash_map.hh>
#include <vull/container/vector.hh>
#include <vull/shaderc/ast.hh>
#include <vull/shaderc/token.hh>
#include <vull/shaderc/type.hh>
#include <vull/support/optional.hh>
#include <vull/support/result.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/support/utility.hh>
#include <vull/support/variant.hh>

#include <stdint.h>

namespace vull::shaderc {

class Lexer;

class ParseMessage {
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
    ParseMessage(Kind kind, const Token &token, String &&text)
        : m_token(token), m_text(vull::move(text)), m_kind(kind) {}

    const Token &token() const { return m_token; }
    const String &text() const { return m_text; }
    Kind kind() const { return m_kind; }
};

class ParseError {
    // TODO(small-vector)
    Vector<ParseMessage> m_messages;

public:
    ParseError() = default;
    ParseError(const ParseError &other) { m_messages.extend(other.m_messages); }
    ParseError(ParseError &&) = default;
    ~ParseError() = default;

    ParseError &operator=(const ParseError &) = delete;
    ParseError &operator=(ParseError &&) = delete;

    void add_error(const Token &token, String &&message);
    void add_note(const Token &token, String &&message);
    void add_note_no_line(const Token &token, String &&message);

    const Vector<ParseMessage> &messages() const { return m_messages; }
};

template <typename T>
using ParseResult = Result<ast::NodeHandle<T>, ParseError>;

class Parser {
    using Operand = Variant<ast::NodeHandle<ast::Node>, StringView, Vector<ast::NodeHandle<ast::Node>>>;

public:
    enum class Operator : uint32_t;

private:
    Lexer &m_lexer;
    ast::Root m_root;
    HashMap<StringView, Type> m_builtin_type_map;

    Optional<Token> consume(TokenKind kind);
    Result<Token, ParseError> expect(TokenKind kind);
    Result<Token, ParseError> expect(TokenKind kind, StringView reason);
    Result<void, ParseError> expect_semi(StringView entity_name);

    Result<Type, ParseError> parse_type();

    ParseResult<ast::Node> build_call_or_construct(Vector<Operand> &operands);
    ast::NodeHandle<ast::Node> build_node(Operand operand);
    void build_expr(Operator op, Vector<Operand> &operands);
    Optional<Operand> parse_operand();
    ParseResult<ast::Node> parse_expr();

    ParseResult<ast::Node> parse_stmt();
    ParseResult<ast::Aggregate> parse_block();

    ParseResult<ast::FunctionDecl> parse_function_decl();
    ParseResult<ast::PipelineDecl> parse_pipeline_decl();
    ParseResult<ast::Aggregate> parse_uniform_block();
    ParseResult<ast::Node> parse_top_level();

public:
    explicit Parser(Lexer &lexer);

    Result<ast::Root, ParseError> parse();
};

} // namespace vull::shaderc
