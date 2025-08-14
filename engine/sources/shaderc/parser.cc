#include <vull/shaderc/parser.hh>

#include <vull/container/array.hh>
#include <vull/container/hash_map.hh>
#include <vull/container/vector.hh>
#include <vull/shaderc/ast.hh>
#include <vull/shaderc/error.hh>
#include <vull/shaderc/lexer.hh>
#include <vull/shaderc/source_location.hh>
#include <vull/shaderc/token.hh>
#include <vull/shaderc/tree.hh>
#include <vull/shaderc/type.hh>
#include <vull/support/assert.hh>
#include <vull/support/enum.hh>
#include <vull/support/optional.hh>
#include <vull/support/result.hh>
#include <vull/support/string.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/string_view.hh>
#include <vull/support/utility.hh>

#include <stdint.h>

namespace vull::shaderc {

enum class Parser::Operator : uint32_t {
    // Binary arithmetic operators.
    Add,
    Sub,
    Mul,
    Div,
    Mod,

    Assign,
    AddAssign,
    SubAssign,
    MulAssign,
    DivAssign,

    // Unary operators.
    Negate,

    ArgumentSeparator,
    CallOrConstruct,
    OpenParen,

    _count,
};

using Operator = Parser::Operator;

namespace {

enum class ParseState {
    Unary,
    Binary,
};

Array<unsigned, vull::to_underlying(Operator::_count)> s_precedence_table{
    2, 2,          // Add, Sub
    3, 3, 3,       // Mul, Div, Mod
    0, 0, 0, 0, 0, // Assigns
    4,             // Negate
    1,             // ArgumentSeparator
    0, 0,          // CallOrConstruct, OpenParen
};

unsigned precedence_of(Operator op) {
    return s_precedence_table[vull::to_underlying(op)];
}

bool is_right_associative(Operator op) {
    switch (op) {
    case Operator::Assign:
    case Operator::AddAssign:
    case Operator::SubAssign:
    case Operator::MulAssign:
    case Operator::DivAssign:
        return true;
    default:
        return false;
    }
}

bool has_higher_precedence(Operator lhs, Operator rhs) {
    if (is_right_associative(rhs)) {
        return precedence_of(lhs) > precedence_of(rhs);
    }
    return precedence_of(lhs) >= precedence_of(rhs);
}

Optional<Operator> to_op(TokenKind kind, ParseState state) {
    if (state == ParseState::Unary) {
        if (kind == '-'_tk) {
            return Operator::Negate;
        }
        return {};
    }

    switch (kind) {
    case '+'_tk:
        return Operator::Add;
    case '-'_tk:
        return Operator::Sub;
    case '*'_tk:
        return Operator::Mul;
    case '/'_tk:
        return Operator::Div;
    case '%'_tk:
        return Operator::Mod;
    case '='_tk:
        return Operator::Assign;
    case TokenKind::PlusEqual:
        return Operator::AddAssign;
    case TokenKind::MinusEqual:
        return Operator::SubAssign;
    case TokenKind::AsteriskEqual:
        return Operator::MulAssign;
    case TokenKind::SlashEqual:
        return Operator::DivAssign;
    default:
        return {};
    }
}

Error unexpected_token(Token bad_token, StringView expected) {
    Error error;
    error.add_error(bad_token, vull::format("unexpected token {}", bad_token.to_string()));
    error.add_note_no_line(bad_token, expected);
    return error;
}

} // namespace

Parser::Parser(Lexer &lexer) : m_lexer(lexer) {
    m_builtin_type_map.set("float", ScalarType::Float);
    m_builtin_type_map.set("vec2", {ScalarType::Float, 2});
    m_builtin_type_map.set("vec3", {ScalarType::Float, 3});
    m_builtin_type_map.set("vec4", {ScalarType::Float, 4});
    m_builtin_type_map.set("ivec2", {ScalarType::Int, 2});
    m_builtin_type_map.set("ivec3", {ScalarType::Int, 3});
    m_builtin_type_map.set("ivec4", {ScalarType::Int, 4});
    m_builtin_type_map.set("uvec2", {ScalarType::Uint, 2});
    m_builtin_type_map.set("uvec3", {ScalarType::Uint, 3});
    m_builtin_type_map.set("uvec4", {ScalarType::Uint, 4});
    m_builtin_type_map.set("mat3", {ScalarType::Float, 3, 3});
    m_builtin_type_map.set("mat4", {ScalarType::Float, 4, 4});
}

Optional<Token> Parser::consume(TokenKind kind) {
    const auto &token = m_lexer.peek();
    return token.kind() == kind ? m_lexer.next() : Optional<Token>();
}

Result<Token, Error> Parser::expect(TokenKind kind) {
    auto token = m_lexer.next();
    if (token.kind() != kind) {
        Error error;
        error.add_error(token, vull::format("expected {} but got {}", Token::kind_string(kind), token.to_string()));
        return error;
    }
    return token;
}

Result<Token, Error> Parser::expect(TokenKind kind, StringView reason) {
    auto token = m_lexer.next();
    if (token.kind() != kind) {
        Error error;
        error.add_error(m_lexer.cursor_token(), vull::format("expected {} {}", Token::kind_string(kind), reason));
        error.add_note(token, vull::format("got {} instead", token.to_string()));
        return error;
    }
    return token;
}

Result<void, Error> Parser::expect_semi(StringView entity_name) {
    auto token = m_lexer.next();
    if (token.kind() != ';'_tk) {
        Error error;
        error.add_error(m_lexer.cursor_token(), vull::format("missing ';' after {}", entity_name));
        error.add_note(token, vull::format("expected ';' before {}", token.to_string()));
        return error;
    }
    return {};
}

Result<Type, Error> Parser::parse_type() {
    auto token = m_lexer.next();
    if (token.kind() != TokenKind::Identifier) {
        Error error;
        error.add_error(token, vull::format("expected type name but got {}", token.to_string()));
        return error;
    }

    auto builtin_type = m_builtin_type_map.get(token.string());
    if (!builtin_type) {
        Error error;
        error.add_error(token, vull::format("unknown type name '{}'", token.string()));
        return error;
    }
    return *builtin_type;
}

ParseResult<ast::Node> Parser::build_call_or_construct(Vector<Operand> &operands) {
    // We should have at least the name operand and an argument list or a single argument AST node.
    VULL_ASSERT(operands.size() >= 2);

    auto argument_list = operands.take_last();
    if (auto single_argument = argument_list.try_get<ast::NodeHandle<ast::Node>>()) {
        Vector<ast::NodeHandle<ast::Node>> vector;
        vector.push(vull::move(*single_argument));
        argument_list.set(vull::move(vector));
    }

    auto name_operand = operands.take_last();
    if (!name_operand.has<StringView>()) {
        Error error;
        error.add_error(name_operand.location, "expression cannot be used as a function call");
        return error;
    }

    auto arguments = vull::move(argument_list.get<Vector<ast::NodeHandle<ast::Node>>>());
    auto name = name_operand.get<StringView>();

    // Builtin type construction, e.g. vec4(1.0f).
    if (auto type = m_builtin_type_map.get(name)) {
        auto construct_expr = m_root.allocate<ast::Aggregate>(name_operand.location, ast::AggregateKind::ConstructExpr);
        for (auto &argument : arguments) {
            construct_expr->append_node(vull::move(argument));
        }
        construct_expr->set_type(*type);
        return construct_expr;
    }

    // Otherwise a regular function call.
    auto call_expr = m_root.allocate<ast::CallExpr>(name_operand.location, name);
    for (auto &argument : arguments) {
        call_expr->append_argument(vull::move(argument));
    }
    return call_expr;
}

ast::NodeHandle<ast::Node> Parser::build_node(Operand operand) {
    if (auto node = operand.try_get<ast::NodeHandle<ast::Node>>()) {
        return vull::move(*node);
    }
    return m_root.allocate<ast::Symbol>(operand.location, operand.get<StringView>());
}

static ast::BinaryOp to_binary_op(Operator op) {
    switch (op) {
    case Operator::Add:
        return ast::BinaryOp::Add;
    case Operator::Sub:
        return ast::BinaryOp::Sub;
    case Operator::Mul:
        return ast::BinaryOp::Mul;
    case Operator::Div:
        return ast::BinaryOp::Div;
    case Operator::Mod:
        return ast::BinaryOp::Mod;
    case Operator::Assign:
        return ast::BinaryOp::Assign;
    case Operator::AddAssign:
        return ast::BinaryOp::AddAssign;
    case Operator::SubAssign:
        return ast::BinaryOp::SubAssign;
    case Operator::MulAssign:
        return ast::BinaryOp::MulAssign;
    case Operator::DivAssign:
        return ast::BinaryOp::DivAssign;
    default:
        VULL_ENSURE_NOT_REACHED();
    }
}

void Parser::build_expr(Operator op, Vector<Operand> &operands) {
    // Handle unary operators first.
    auto rhs = build_node(operands.take_last());
    if (op == Operator::Negate) {
        operands.emplace(m_root.allocate<ast::UnaryExpr>(ast::UnaryOp::Negate, vull::move(rhs)));
        return;
    }

    // Special handling for argument separator (comma).
    if (op == Operator::ArgumentSeparator) {
        // Check if we already have an arguments vector.
        if (auto vector = operands.last().try_get<Vector<ast::NodeHandle<ast::Node>>>()) {
            vector->push(vull::move(rhs));
            return;
        }

        // Otherwise this is the second argument and we need to create a vector.
        auto lhs = build_node(operands.take_last());
        auto location = lhs->source_location();
        Vector<ast::NodeHandle<ast::Node>> arguments;
        arguments.push(vull::move(lhs));
        arguments.push(vull::move(rhs));
        operands.emplace(vull::move(arguments), location);
        return;
    }

    // Otherwise op is a binary operator.
    auto lhs = build_node(operands.take_last());
    operands.emplace(m_root.allocate<ast::BinaryExpr>(to_binary_op(op), vull::move(lhs), vull::move(rhs)));
}

Optional<Parser::Operand> Parser::parse_operand() {
    if (auto literal = consume(TokenKind::FloatLit)) {
        return Operand(m_root.allocate<ast::Constant>(literal->location(), literal->decimal()));
    }
    if (auto literal = consume(TokenKind::IntLit)) {
        return Operand(m_root.allocate<ast::Constant>(literal->location(), literal->integer()));
    }
    if (auto identifier = consume(TokenKind::Identifier)) {
        return Operand(identifier->string(), identifier->location());
    }
    return {};
}

// Implementation of The Double-E Infix Expression Parsing Method.
// See https://github.com/erikeidt/erikeidt.github.io/blob/master/The-Double-E-Method.md
ParseResult<ast::Node> Parser::parse_expr() {
    // The parser is in one of two states. In the unary state we are looking for a unary operator or an operand. In the
    // binary state we are looking for a binary operator or the end of the expression.

    // We use an Operator enum to use with the current stack of operators which differentiates between, for example,
    // unary negation and binary subtraction which both use the same lexical token.
    // TODO(small-vector)
    Vector<Operand> operands;
    Vector<Operator> operators;

    auto reduce_top_operator = [&] -> Result<void, Error> {
        auto op = operators.take_last();
        if (op == Operator::CallOrConstruct || op == Operator::OpenParen) {
            Error error;
            error.add_error(m_lexer.cursor_token(), "unmatched '('");
            return error;
        }
        build_expr(op, operands);
        return {};
    };

    auto reduce_by_precedence = [&](Operator op) -> Result<void, Error> {
        while (!operators.empty() && has_higher_precedence(operators.last(), op)) {
            VULL_TRY(reduce_top_operator());
        }
        return {};
    };

    // Start in the unary state.
    auto state = ParseState::Unary;
    while (true) {
        const auto peeked_token = m_lexer.peek();
        if (auto op = to_op(peeked_token.kind(), state)) {
            // Consume the operator token.
            m_lexer.next();

            // If in the unary state, push the operator onto the stack and stay in the unary state.
            if (state == ParseState::Unary) {
                operators.push(*op);
                continue;
            }

            // Otherwise we're in the binary state and need to reduce the operator stack before pushing the new
            // operator and switching back to the unary state.
            VULL_TRY(reduce_by_precedence(*op));
            operators.push(*op);
            state = ParseState::Unary;
            continue;
        }

        if (auto operand = parse_operand()) {
            // Seeing an operand in the binary state means an operator was missed.
            if (state == ParseState::Binary) {
                Error error;
                error.add_error(peeked_token, "unexpected expression part");
                error.add_note_no_line(peeked_token, "expected operator or end of expression");
                return error;
            }
            operands.push(vull::move(*operand));
            state = ParseState::Binary;
            continue;
        }

        if (state == ParseState::Binary && consume(','_tk)) {
            VULL_TRY(reduce_by_precedence(Operator::ArgumentSeparator));
            if (operators.empty() || operators.last() != Operator::CallOrConstruct) {
                return unexpected_token(peeked_token, "not in a function call context");
            }
            operators.push(Operator::ArgumentSeparator);
            state = ParseState::Unary;
            continue;
        }

        if (consume('('_tk)) {
            // If we're in the unary state, this is a grouping parenthesis.
            if (state == ParseState::Unary) {
                operators.push(Operator::OpenParen);
                continue;
            }

            // Otherwise, it is function-call-like.
            operators.push(Operator::CallOrConstruct);
            state = ParseState::Unary;
            continue;
        }

        if (auto closing_paren = consume(')'_tk)) {
            // If we're in the unary state, we should only accept a closing parenthesis in the edge case of an empty
            // argument list to a call or construction expression.
            if (state == ParseState::Unary) {
                if (operators.empty() || operators.last() != Operator::CallOrConstruct) {
                    return unexpected_token(*closing_paren, "expected expression part");
                }

                // Push an empty argument list beforehand.
                operators.pop();
                operands.emplace(Vector<ast::NodeHandle<ast::Node>>(), closing_paren->location());
                operands.push(VULL_TRY(build_call_or_construct(operands)));
                state = ParseState::Binary;
                continue;
            }

            // Reduce until we find a matching open parenthesis from either a grouping or a call or construction
            // expression.
            while (true) {
                if (operators.empty()) {
                    return unexpected_token(*closing_paren, "expected operator or end of expression");
                }
                auto op = operators.last();

                // Build call or construction node.
                if (op == Operator::CallOrConstruct) {
                    operators.pop();
                    operands.push(VULL_TRY(build_call_or_construct(operands)));
                    break;
                }

                // Consume grouping parenthesis.
                if (op == Operator::OpenParen) {
                    operators.pop();
                    break;
                }

                VULL_TRY(reduce_top_operator());
            }
            continue;
        }

        // We've reached the end of the expression.
        if (state == ParseState::Unary) {
            auto next_token = m_lexer.next();
            Error error;
            error.add_error(m_lexer.cursor_token(), "reached unexpected end of expression");
            error.add_note(next_token, vull::format("expected expression part before {}", next_token.to_string()));
            return error;
        }
        if (state == ParseState::Binary) {
            break;
        }
    }

    // Final reduction of the operator stack.
    while (!operators.empty()) {
        VULL_TRY(reduce_top_operator());
    }
    return build_node(operands.take_last());
}

ParseResult<ast::Node> Parser::parse_stmt() {
    if (consume(TokenKind::KW_let)) {
        auto name = VULL_TRY(expect(TokenKind::Identifier));
        Type type;
        if (consume(':'_tk)) {
            type = VULL_TRY(parse_type());
        }
        VULL_TRY(expect('='_tk, "for value"));
        auto value = VULL_TRY(parse_expr());
        VULL_TRY(expect_semi("let statement"));
        return m_root.allocate<ast::DeclStmt>(name.location(), name.string(), type, vull::move(value), false);
    }

    if (auto keyword = consume(TokenKind::KW_var)) {
        auto name = VULL_TRY(expect(TokenKind::Identifier));
        Type type;
        if (consume(':'_tk)) {
            type = VULL_TRY(parse_type());
        }
        ast::NodeHandle<ast::Node> value;
        if (consume('='_tk)) {
            value = VULL_TRY(parse_expr());
        }
        VULL_TRY(expect_semi("var statement"));
        if (!type.is_valid() && !value) {
            Error error;
            error.add_error(*keyword, "Declaration must have an explicit type or an initial value");
            return error;
        }
        return m_root.allocate<ast::DeclStmt>(name.location(), name.string(), type, vull::move(value), true);
    }

    // Freestanding expression.
    auto expr = VULL_TRY(parse_expr());
    if (consume(';'_tk)) {
        return expr;
    }

    // No semicolon, implicit return.
    const auto location = expr->source_location();
    return m_root.allocate<ast::ReturnStmt>(location, vull::move(expr));
}

ParseResult<ast::Aggregate> Parser::parse_block() {
    auto open_brace = VULL_TRY(expect('{'_tk, "to open a block"));
    auto block = m_root.allocate<ast::Aggregate>(open_brace.location(), ast::AggregateKind::Block);
    while (!consume('}'_tk)) {
        block->append_node(VULL_TRY(parse_stmt()));
    }
    return vull::move(block);
}

ParseResult<ast::FunctionDecl> Parser::parse_function_decl(SourceLocation location) {
    auto name = VULL_TRY(expect(TokenKind::Identifier, "for function name"));
    VULL_TRY(expect('('_tk, "to open the parameter list"));

    Vector<ast::Parameter> parameters;
    while (!consume(')'_tk)) {
        if (!consume(TokenKind::KW_let)) {
            return unexpected_token(m_lexer.next(), "expected a parameter (let) or ')'");
        }
        auto param_name = VULL_TRY(expect(TokenKind::Identifier, "for parameter name"));
        VULL_TRY(expect(':'_tk));
        auto param_type = VULL_TRY(parse_type());
        parameters.emplace(param_name.location(), param_name.string(), param_type);
        consume(','_tk);
    }

    Type return_type(ScalarType::Void);
    if (consume(':'_tk)) {
        return_type = VULL_TRY(parse_type());
    }

    auto block = VULL_TRY(parse_block());
    return m_root.allocate<ast::FunctionDecl>(location, name.string(), vull::move(block), return_type,
                                              vull::move(parameters));
}

ParseResult<ast::IoDecl> Parser::parse_io_decl(SourceLocation location, ast::IoKind io_kind) {
    ast::NodeHandle<ast::Node> symbol_or_block;
    if (auto open_brace = consume('{'_tk)) {
        if (io_kind == ast::IoKind::Pipeline) {
            Error error;
            error.add_error(*open_brace, "a pipeline declaration cannot be a block");
            return error;
        }
        auto block = m_root.allocate<ast::Aggregate>(open_brace->location(), ast::AggregateKind::Block);
        while (!consume('}'_tk)) {
            auto name = VULL_TRY(expect(TokenKind::Identifier));
            VULL_TRY(expect(':'_tk));
            auto type = VULL_TRY(parse_type());
            block->append_node(m_root.allocate<ast::Symbol>(name.location(), name.string(), type));
            VULL_TRY(expect(';'_tk));
        }
        symbol_or_block = vull::move(block);
    }

    if (!symbol_or_block) {
        auto type = VULL_TRY(parse_type());
        auto name = VULL_TRY(expect(TokenKind::Identifier));
        symbol_or_block = m_root.allocate<ast::Symbol>(name.location(), name.string(), type);
    }
    VULL_TRY(expect_semi("IO declaration"));
    return m_root.allocate<ast::IoDecl>(location, io_kind, vull::move(symbol_or_block));
}

ParseResult<ast::Node> Parser::parse_top_level() {
    if (auto keyword = consume(TokenKind::KW_fn)) {
        return VULL_TRY(parse_function_decl(keyword->location()));
    }
    if (auto keyword = consume(TokenKind::KW_pipeline)) {
        return VULL_TRY(parse_io_decl(keyword->location(), ast::IoKind::Pipeline));
    }
    if (auto keyword = consume(TokenKind::KW_uniform)) {
        return VULL_TRY(parse_io_decl(keyword->location(), ast::IoKind::Uniform));
    }
    return unexpected_token(m_lexer.next(), "expected top level declaration or <eof>");
}

ParseResult<ast::Node> Parser::parse_attribute() {
    auto name = VULL_TRY(expect(TokenKind::Identifier, "for attribute name"));
    if (name.string() != "push_constant") {
        Error error;
        error.add_error(name, vull::format("unknown attribute '{}'", name.string()));
        return error;
    }
    return m_root.allocate<ast::Attribute>(ast::NodeKind::PushConstant, name.location());
}

Result<ast::Root, Error> Parser::parse() {
    while (!consume(TokenKind::Eof)) {
        Vector<ast::NodeHandle<ast::Node>> attributes;
        if (consume(TokenKind::DoubleOpenSquareBrackets)) {
            while (true) {
                attributes.push(VULL_TRY(parse_attribute()));
                if (!consume(','_tk)) {
                    break;
                }
            }
            VULL_TRY(expect(TokenKind::DoubleCloseSquareBrackets, "to end attribute list"));
        }
        auto node = VULL_TRY(parse_top_level());
        node->set_attributes(vull::move(attributes));
        m_root.append_top_level(vull::move(node));
    }
    return vull::move(m_root);
}

} // namespace vull::shaderc
